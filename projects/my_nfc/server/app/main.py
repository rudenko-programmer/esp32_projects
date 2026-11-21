"""NFC webhook receiver.

Принимает сканы от ESP32 (POST /webhook/nfc), хранит их в SQLite,
транслирует в реальном времени на веб-страницу (Server-Sent Events),
позволяет давать картам имена и опционально пересылает события дальше
(например, в n8n или Laravel API).
"""

from __future__ import annotations

import asyncio
import json
import logging
import os
import secrets
import sqlite3
from contextlib import asynccontextmanager, contextmanager
from datetime import datetime, timezone
from pathlib import Path as FsPath
from typing import AsyncIterator, Iterator

import httpx
from fastapi import BackgroundTasks, Depends, FastAPI, Header, HTTPException, Path, Query, Request
from fastapi.concurrency import run_in_threadpool
from fastapi.responses import FileResponse, StreamingResponse
from pydantic import BaseModel, ConfigDict, Field

API_KEY = os.getenv("API_KEY", "").strip()
DB_PATH = os.getenv("DB_PATH", "/data/nfc.db")
FORWARD_URL = os.getenv("FORWARD_URL", "").strip()
FORWARD_TIMEOUT = float(os.getenv("FORWARD_TIMEOUT", "5"))
SSE_PING_SECONDS = 15

INDEX_HTML = FsPath(__file__).parent / "static" / "index.html"

logging.basicConfig(
    level=os.getenv("LOG_LEVEL", "INFO").upper(),
    format="%(asctime)s %(levelname)s %(name)s: %(message)s",
)
log = logging.getLogger("nfc-webhook")

UID_PATTERN = r"^[0-9A-Fa-f]{8,20}$"  # 4..10 байт в hex

SCHEMA = """
CREATE TABLE IF NOT EXISTS scans (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    received_at TEXT NOT NULL,
    device_id   TEXT NOT NULL,
    uid         TEXT NOT NULL,
    client_ip   TEXT,
    payload     TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_scans_uid ON scans(uid);
CREATE TABLE IF NOT EXISTS cards (
    uid        TEXT PRIMARY KEY,
    name       TEXT NOT NULL,
    created_at TEXT NOT NULL
);
"""


# ---------------------------------------------------------------------
#  DB
# ---------------------------------------------------------------------
@contextmanager
def db() -> Iterator[sqlite3.Connection]:
    conn = sqlite3.connect(DB_PATH, timeout=10)
    conn.row_factory = sqlite3.Row
    try:
        with conn:  # commit / rollback
            yield conn
    finally:
        conn.close()


def utcnow() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


# ---------------------------------------------------------------------
#  Live events (Server-Sent Events)
# ---------------------------------------------------------------------
class Broker:
    """Раздаёт события всем открытым вкладкам браузера. Вызывать только из event loop."""

    def __init__(self) -> None:
        self._subscribers: set[asyncio.Queue] = set()

    def subscribe(self) -> asyncio.Queue:
        queue: asyncio.Queue = asyncio.Queue(maxsize=100)
        self._subscribers.add(queue)
        return queue

    def unsubscribe(self, queue: asyncio.Queue) -> None:
        self._subscribers.discard(queue)

    def publish(self, event_type: str, data: dict) -> None:
        for queue in list(self._subscribers):
            try:
                queue.put_nowait((event_type, data))
            except asyncio.QueueFull:
                pass  # медленный клиент — пропускаем событие, при переподключении он перечитает список

    @property
    def clients(self) -> int:
        return len(self._subscribers)


broker = Broker()


@asynccontextmanager
async def lifespan(_: FastAPI):
    os.makedirs(os.path.dirname(DB_PATH) or ".", exist_ok=True)
    with db() as conn:
        conn.executescript(SCHEMA)
    if not API_KEY:
        log.warning("API_KEY is not set: webhook accepts requests without authentication")
    log.info("DB: %s, forward: %s", DB_PATH, FORWARD_URL or "disabled")
    yield


app = FastAPI(title="NFC Webhook Receiver", version="1.1.0", lifespan=lifespan)


# ---------------------------------------------------------------------
#  Auth
# ---------------------------------------------------------------------
def require_api_key(x_api_key: str | None = Header(default=None)) -> None:
    if not API_KEY:
        return
    if not x_api_key or not secrets.compare_digest(x_api_key, API_KEY):
        raise HTTPException(status_code=401, detail="invalid or missing X-API-Key")


# ---------------------------------------------------------------------
#  Models
# ---------------------------------------------------------------------
class ScanIn(BaseModel):
    model_config = ConfigDict(extra="allow")  # доп. поля от прошивки сохраняются как есть

    uid: str = Field(pattern=UID_PATTERN, description="UID карты в hex")
    device_id: str = Field(default="unknown", max_length=64)
    uid_length: int | None = None
    card_type: str | None = Field(default=None, max_length=64, description="Тип карты, например MIFARE Classic 1K")
    atqa: str | None = Field(default=None, pattern=r"^[0-9A-Fa-f]{4}$", description="ATQA (SENS_RES), hex")
    sak: str | None = Field(default=None, pattern=r"^[0-9A-Fa-f]{2}$", description="SAK (SEL_RES), hex")
    firmware: str | None = None
    rssi: int | None = None
    uptime_ms: int | None = None


class CardIn(BaseModel):
    name: str = Field(min_length=1, max_length=128)


# ---------------------------------------------------------------------
#  Forwarding
# ---------------------------------------------------------------------
async def forward_event(event: dict) -> None:
    try:
        async with httpx.AsyncClient(timeout=FORWARD_TIMEOUT) as client:
            resp = await client.post(FORWARD_URL, json=event)
        if resp.is_success:
            log.info("forwarded scan %s -> %s", event.get("scan_id"), resp.status_code)
        else:
            log.warning("forward returned %s: %s", resp.status_code, resp.text[:200])
    except httpx.HTTPError as exc:
        log.warning("forward failed: %r", exc)


# ---------------------------------------------------------------------
#  Sync DB helpers (выполняются в threadpool)
# ---------------------------------------------------------------------
def save_scan(scan: ScanIn, client_ip: str | None) -> dict:
    uid = scan.uid.upper()
    received_at = utcnow()
    payload = scan.model_dump()
    payload["uid"] = uid

    with db() as conn:
        cur = conn.execute(
            "INSERT INTO scans (received_at, device_id, uid, client_ip, payload) VALUES (?, ?, ?, ?, ?)",
            (received_at, scan.device_id, uid, client_ip, json.dumps(payload, ensure_ascii=False)),
        )
        card = conn.execute("SELECT name FROM cards WHERE uid = ?", (uid,)).fetchone()

    name = card["name"] if card else None
    return {
        "id": cur.lastrowid,
        "received_at": received_at,
        "device_id": scan.device_id,
        "uid": uid,
        "client_ip": client_ip,
        "name": name,
        "known": name is not None,
        "payload": payload,
    }


def save_card(uid: str, name: str) -> None:
    with db() as conn:
        conn.execute(
            "INSERT INTO cards (uid, name, created_at) VALUES (?, ?, ?) "
            "ON CONFLICT(uid) DO UPDATE SET name = excluded.name",
            (uid, name, utcnow()),
        )


def remove_card(uid: str) -> int:
    with db() as conn:
        return conn.execute("DELETE FROM cards WHERE uid = ?", (uid,)).rowcount


# ---------------------------------------------------------------------
#  Routes
# ---------------------------------------------------------------------
@app.get("/", include_in_schema=False)
def index() -> FileResponse:
    return FileResponse(INDEX_HTML, headers={"Cache-Control": "no-cache"})


@app.get("/health")
def health() -> dict:
    return {"status": "ok", "live_clients": broker.clients}


@app.post("/webhook/nfc", dependencies=[Depends(require_api_key)])
async def receive_scan(scan: ScanIn, request: Request, background: BackgroundTasks) -> dict:
    client_ip = request.client.host if request.client else None
    event = await run_in_threadpool(save_scan, scan, client_ip)
    broker.publish("scan", event)

    log.info(
        "scan #%s uid=%s device=%s known=%s ip=%s",
        event["id"], event["uid"], event["device_id"], event["known"], client_ip,
    )

    result = {
        "status": "ok",
        "scan_id": event["id"],
        "uid": event["uid"],
        "known": event["known"],
        "name": event["name"],
    }
    if FORWARD_URL:
        background.add_task(forward_event, {**event["payload"], **result, "received_at": event["received_at"]})
    return result


@app.get("/api/events")
async def events(request: Request) -> StreamingResponse:
    """Поток событий для веб-страницы: `scan` — новый скан, `card` — карте дали/убрали имя."""
    queue = broker.subscribe()

    async def stream() -> AsyncIterator[str]:
        try:
            yield "retry: 3000\n\n"
            while not await request.is_disconnected():
                try:
                    event_type, data = await asyncio.wait_for(queue.get(), timeout=SSE_PING_SECONDS)
                except asyncio.TimeoutError:
                    yield ": ping\n\n"  # keep-alive, чтобы прокси не рвали соединение
                    continue
                yield f"event: {event_type}\ndata: {json.dumps(data, ensure_ascii=False)}\n\n"
        finally:
            broker.unsubscribe(queue)

    return StreamingResponse(
        stream(),
        media_type="text/event-stream",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"},
    )


@app.get("/api/scans")
def list_scans(
    limit: int = Query(50, ge=1, le=1000),
    uid: str | None = Query(None, pattern=UID_PATTERN),
    device_id: str | None = None,
) -> list[dict]:
    sql = (
        "SELECT s.id, s.received_at, s.device_id, s.uid, s.client_ip, s.payload, c.name "
        "FROM scans s LEFT JOIN cards c ON c.uid = s.uid WHERE 1=1"
    )
    params: list = []
    if uid:
        sql += " AND s.uid = ?"
        params.append(uid.upper())
    if device_id:
        sql += " AND s.device_id = ?"
        params.append(device_id)
    sql += " ORDER BY s.id DESC LIMIT ?"
    params.append(limit)

    with db() as conn:
        rows = conn.execute(sql, params).fetchall()
    return [
        {**dict(r), "known": r["name"] is not None, "payload": json.loads(r["payload"])}
        for r in rows
    ]


@app.get("/api/stats")
def stats() -> dict:
    with db() as conn:
        row = conn.execute(
            "SELECT (SELECT COUNT(*) FROM scans) AS scans, "
            "(SELECT COUNT(DISTINCT uid) FROM scans) AS unique_uids, "
            "(SELECT COUNT(*) FROM cards) AS named_cards, "
            "(SELECT COUNT(DISTINCT device_id) FROM scans) AS devices"
        ).fetchone()
    return dict(row)


@app.get("/api/cards")
def list_cards() -> list[dict]:
    with db() as conn:
        rows = conn.execute(
            "SELECT c.uid, c.name, c.created_at, "
            "(SELECT MAX(received_at) FROM scans s WHERE s.uid = c.uid) AS last_seen "
            "FROM cards c ORDER BY c.name"
        ).fetchall()
    return [dict(r) for r in rows]


@app.put("/api/cards/{uid}", dependencies=[Depends(require_api_key)])
async def upsert_card(card: CardIn, uid: str = Path(pattern=UID_PATTERN)) -> dict:
    uid = uid.upper()
    name = card.name.strip()
    await run_in_threadpool(save_card, uid, name)
    broker.publish("card", {"uid": uid, "name": name})
    return {"uid": uid, "name": name}


@app.delete("/api/cards/{uid}", dependencies=[Depends(require_api_key)])
async def delete_card(uid: str = Path(pattern=UID_PATTERN)) -> dict:
    uid = uid.upper()
    if not await run_in_threadpool(remove_card, uid):
        raise HTTPException(status_code=404, detail="card not found")
    broker.publish("card", {"uid": uid, "name": None})
    return {"status": "deleted"}
