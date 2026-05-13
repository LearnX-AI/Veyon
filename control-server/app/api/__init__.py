"""API routers - wires up all endpoint modules under /api/v1."""

from fastapi import APIRouter

from app.api import admin, blocklist, files, folders, machines


api_router = APIRouter(prefix="/api/v1")
api_router.include_router(blocklist.router)
api_router.include_router(machines.router)
api_router.include_router(files.router)
api_router.include_router(folders.router)
api_router.include_router(admin.router)
