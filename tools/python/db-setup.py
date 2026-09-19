import csv
import numpy as np
import pandas as pd
import psycopg
from psycopg.rows import dict_row


__LINES_CSV = "Train Station Data - Route.csv"
__STATIONS_CSV = "Train Station Data - Station.csv"


def get_connection():
    __DB_CONFIG = {
        "host": "localhost",
        "port": "5432",
        "dbname": "jrhyaku",
        "user": "developer"
    }

    return psycopg.connect(**__DB_CONFIG, row_factory=dict_row)


def calc_transformation_matrix():
    geo_points = np.array([
        [141.350768, 43.068612], # Sapporo Sta. (lon, lat)
        [139.766103, 35.681391], # Tokyo Sta. (lon, lat)
        [130.460447, 33.542092] # Fukuoka Sta. (lon, lat)
    ])
    
    svg_points = np.array([
        [459.605, 99.773], # Sapporo's pixel position
        [423.699, 303.048], # Tokyo's pixel position
        [236.145, 350.184] # Fukuoka's pixel position
    ])

    # Tranform is 3x2 = [lon_coeff, lat_coeff, offset] for x,y
    A_geo = np.hstack([geo_points, np.ones((len(geo_points), 1))])
    transform, *_ = np.linalg.lstsq(A_geo, svg_points, rcond=None)
    return transform


def geo_to_svg(lon, lat, transform):
    vec = np.array([lon, lat, 1.0])
    x, y = vec @ transform
    return x, y


def svg_to_png_pixel(x, y, bbox, out_w=128, out_h=128):
    min_x, min_y, max_x, max_y = bbox
    svg_width = max_x - min_x
    svg_height = max_y - min_y

    alpha_w = out_w / svg_width
    alpha_h = out_h / svg_height
    alpha = min(alpha_w, alpha_h)

    content_w = svg_width * alpha
    content_h = svg_height * alpha
    offset_x = (out_w - content_w) / 2.0
    offset_y = (out_h - content_h) / 2.0

    px = (x - min_x) * alpha + offset_x
    py = (y - min_y) * alpha + offset_y
    return px, py


def calc_line_bounding_box(df, transform):
    PADDING_FRACTION = 0.05     # % of the bbox's own width/height; const padding could be used, too
    MIN_BOX_SIZE = 10.0         # guards against small boxes

    northernmost_sta = df.loc[df['lat'].idxmax()]
    southernmost_sta = df.loc[df['lat'].idxmin()]
    easternmost_sta = df.loc[df['lon'].idxmax()]
    westernmost_sta = df.loc[df['lon'].idxmin()]

    corners_geo = [
        (westernmost_sta['lon'], northernmost_sta['lat']),
        (easternmost_sta['lon'], northernmost_sta['lat']),
        (easternmost_sta['lon'], southernmost_sta['lat']),
        (westernmost_sta['lon'], southernmost_sta['lat']),
    ]
    corners_svg = [geo_to_svg(lon, lat, transform) for lon, lat in corners_geo]
    xs = [c[0] for c in corners_svg]
    ys = [c[1] for c in corners_svg]

    min_x, max_x = min(xs), max(xs)
    min_y, max_y = min(ys), max(ys)

    width = max_x - min_x
    height = max_y - min_y

    pad_x = width * PADDING_FRACTION
    pad_y = height * PADDING_FRACTION

    min_x -= pad_x; max_x += pad_x
    min_y -= pad_y; max_y += pad_y

    if (max_x - min_x) < MIN_BOX_SIZE:
        cx = (max_x + min_x) / 2
        min_x, max_x = cx - MIN_BOX_SIZE / 2, cx + MIN_BOX_SIZE / 2
    if (max_y - min_y) < MIN_BOX_SIZE:
        cy = (max_y + min_y) / 2
        min_y, max_y = cy - MIN_BOX_SIZE / 2, cy + MIN_BOX_SIZE / 2

    return f"{min_x:.3f},{min_y:.3f},{max_x:.3f},{max_y:.3f}"


def calc_bboxes_by_line() -> dict[int, str]:
    stations_df = pd.read_csv(__STATIONS_CSV)
    transform = calc_transformation_matrix()
 
    bboxes = (
        stations_df
        .groupby('line_cd')
        .apply(lambda df: calc_line_bounding_box(df, transform))
    )
    return bboxes.to_dict()


def insert_lines(conn, bboxes_by_line: dict[int, str]):
    with open(__LINES_CSV, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        rows = [
            (int(row["line_cd"]), row["line_name"])
            for row in reader
        ]
 
    inserted = 0
    skipped = 0
    missing_bbox = 0
    with conn.cursor() as cur:
        for line_cd, line_name in rows:
            bbox = bboxes_by_line.get(line_cd)
            if bbox is None:
                print(f"  No stations found for line_cd={line_cd} ({line_name}); bbox will be NULL")
                missing_bbox += 1

            cur.execute(
                """
                INSERT INTO lines (line_cd, line_name, line_bbox)
                VALUES (%s, %s, %s)
                ON CONFLICT (line_cd) DO NOTHING
                RETURNING pk
                """,
                (line_cd, line_name, bbox),
            )
            if cur.fetchone() is None:
                print(f"  Skipped duplicate line: line_cd={line_cd} ({line_name})")
                skipped += 1
            else:
                inserted += 1
 
    conn.commit()
    print(f"Loaded {inserted} lines ({skipped} duplicates skipped, {missing_bbox} missing bbox).")
 
 
def insert_stations(conn):
    with open(__STATIONS_CSV, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        rows = [
            (
                int(row["station_cd"]),
                row["station_name"],
                float(row["lon"]),
                float(row["lat"]),
                int(row["line_cd"]),
            )
            for row in reader
        ]
 
    inserted = 0
    skipped = 0
    with conn.cursor() as cur:
        for station_cd, station_name, lon, lat, line_cd in rows:
            cur.execute(
                """
                INSERT INTO stations (station_cd, station_name, lon, lat, line_cd)
                VALUES (%s, %s, %s, %s, %s)
                ON CONFLICT (station_cd) DO NOTHING
                RETURNING pk
                """,
                (station_cd, station_name, lon, lat, line_cd),
            )
            if cur.fetchone() is None:
                print(f"  Skipped duplicate station: station_cd={station_cd} ({station_name})")
                skipped += 1
            else:
                inserted += 1
 
    conn.commit()
    print(f"Inserted {inserted} stations ({skipped} duplicates skipped).")


def main():
    bboxes_by_line = calc_bboxes_by_line()
    with get_connection() as conn:
        insert_lines(conn, bboxes_by_line)
        insert_stations(conn)


if __name__ == '__main__':
    main()
