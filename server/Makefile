VENV := .venv

.PHONY: setup run test lint
setup:
	python3 -m venv $(VENV)
	$(VENV)/bin/pip install -e '.[dev]'

run:
	$(VENV)/bin/uvicorn app.main:app --reload

test:
	$(VENV)/bin/pytest

lint:
	$(VENV)/bin/ruff check .
