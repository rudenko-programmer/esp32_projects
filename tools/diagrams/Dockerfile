# Образ для схем монорепы: schemdraw + WireViz (Graphviz) + JupyterLab.
# Сборка и запуск — через tools/diagrams.ps1, вручную ничего ставить не нужно.
FROM python:3.12-slim

# graphviz        — движок раскладки для WireViz
# fonts-liberation — шрифты с кириллицей, по метрикам совпадают с Arial/Times
# librsvg2-bin    — rsvg-convert: SVG -> PNG для предпросмотра
RUN apt-get update \
 && apt-get install -y --no-install-recommends graphviz fonts-liberation librsvg2-bin \
 && rm -rf /var/lib/apt/lists/*

COPY requirements.txt /tmp/requirements.txt
RUN pip install --no-cache-dir -r /tmp/requirements.txt

WORKDIR /repo
EXPOSE 8888

# По умолчанию — JupyterLab без токена. Порт публикуется только на 127.0.0.1 (см. diagrams.ps1).
CMD ["jupyter", "lab", "--ip=0.0.0.0", "--port=8888", "--no-browser", "--allow-root", \
     "--IdentityProvider.token=", "--ServerApp.root_dir=/repo"]
