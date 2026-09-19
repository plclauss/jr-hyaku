from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from datetime import datetime
import db, display

app = FastAPI()


class Line(BaseModel):
    pk: int
    line_cd: int
    line_name: str
    route_color: str | None
    line_type: int | None
    completion_pct: float


class LineCreate(BaseModel):
    line_cd: int
    line_name: str
    route_color: str | None = None
    line_type: int | None = None


class LineUpdate(BaseModel):
    line_name: str | None = None
    route_color: str | None = None
    line_type: int | None = None


class Station(BaseModel):
    pk: int
    station_cd: int
    station_name: str
    lon: float
    lat: float
    visited_at: datetime | None
    line_cd: int


class StationCreate(BaseModel):
    station_cd: int
    station_name: str
    lon: float
    lat: float


class StationUpdate(BaseModel):
    station_name: str | None = None
    lon: float | None = None
    lat: float | None = None
    visited_at: datetime | None = None


@app.get("/status")
async def get_status():
    return { "status": "API active!" }


@app.get("/lines", response_model=list[Line])
async def get_lines():
    return db.fetch_lines()


@app.get("/lines/{line_cd}", response_model=Line)
async def get_line(line_cd: int):
    line = db.fetch_line(line_cd)
    if line is None:
        raise HTTPException(status_code=404, detail=f"line_cd={line_cd} not found")
    
    display.send_display_command({
        "cmd": "show_line",
        "line_name": line["line_name"],
        "completion_pct": line["completion_pct"]
        # TODO: Add SVG -> PNG pixel transformation for stations.
    })

    return line


@app.get("/lines/{line_cd}/stations", response_model=list[Station])
async def get_line_stations(line_cd: int):
    line = db.fetch_line(line_cd)
    if line is None:
        raise HTTPException(status_code=404, detail=f"line_cd={line_cd} not found")
    return db.fetch_stations_on_line(line_cd)


@app.get("/stations", response_model=list[Station])
async def get_stations():
    return db.fetch_stations()


@app.get("/stations/{station_cd}", response_model=Station)
async def get_station(station_cd: int):
    station = db.fetch_station(station_cd)
    if station is None:
        raise HTTPException(status_code=404, detail=f"station_cd={station_cd} not found")
    return station


@app.post("/lines", response_model=Line, status_code=201)
async def add_line(line: LineCreate):
    try:
        return db.insert_line(line)
    except ValueError as e:
        raise HTTPException(status_code=409, detail=str(e))


@app.post("/lines/{line_cd}/stations", response_model=Station, status_code=201)
async def add_station(line_cd: int, station: StationCreate):
    line = db.fetch_line(line_cd)
    if line is None:
        raise HTTPException(status_code=404, detail=f"line_cd={line_cd} not found")

    try:
        return db.insert_station(line_cd, station)
    except ValueError as e:
        raise HTTPException(status_code=409, detail=str(e))


@app.patch("/lines/{line_cd}", response_model=Line)
async def update_line(line_cd: int, line_update: LineUpdate):
    update_res = db.update_line(line_cd, line_update)
    if update_res is None:
        raise HTTPException(status_code=404, detail=f"Failed to update line_cd={line_cd}")
    return update_res


@app.patch("/stations/{station_cd}", response_model=Station)
async def update_station(station_cd: int, station_update: StationUpdate):
    update_res = db.update_station(station_cd, station_update)
    if update_res is None:
        raise HTTPException(status_code=404, detail=f"Failed to update station_cd={station_cd}")
    return update_res


@app.delete("/lines/{line_cd}", status_code=204)
async def delete_line(line_cd: int):
    if not db.delete_line(line_cd):
        raise HTTPException(status_code=404, detail=f"line_cd={line_cd} not found")


@app.delete("/stations/{station_cd}", status_code=204)
async def delete_station(station_cd: int):
    if not db.delete_station(station_cd):
        raise HTTPException(status_code=404, detail=f"station_cd={station_cd} not found")
