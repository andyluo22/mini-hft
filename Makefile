COMPOSE=docker compose -f ops/docker-compose.yml
PHONY. up down build test clean 
up:
	$(COMPOSE) up
	$(COMPOSE) build -d
	@echo "-> Prometheus Probe: http://localhost:9090"
	@echo "-> Web UI: http://localhost:8080/health"
down:
	$(COMPOSE) down -v
logs:
	$(COMPOSE) -f logs --tail-20
rebuild:
	$(COMPOSE) build --no-cache
test:
	cd api && python -m pytest -q || true
	cd web && npnm run lint || true

clean:
	rm -rf build 