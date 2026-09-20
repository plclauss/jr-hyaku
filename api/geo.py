"""
Imported from tools/python/data-processing.ipynb; see that file for more info.
"""
import numpy as np


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
