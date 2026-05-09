"""API routers."""

from fastapi import APIRouter

from app.api import blocklist


api_router = APIRouter(prefix="/api/v1")
api_router.include_router(blocklist.router)
