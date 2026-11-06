# ChatNav server

FastAPI backend. Python 3.11+.

    cp .env.example .env
    make setup
    make run     # http://127.0.0.1:8000/v1/health
    make test
    make lint

Values live in `app/config.py` and can be overridden in `.env`.
