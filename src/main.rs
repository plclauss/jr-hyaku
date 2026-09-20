use std::env;
use std::fs;
use std::error::Error;
use resvg::tiny_skia as ts;

struct UserInput {
    svg_fp: String,
    constraint_width: i32,
    constraint_height: i32,
    crop: Option<(f32, f32, f32, f32)>,
    out_fp: Option<String>
}

fn collect_args() -> Result<UserInput, String> {
    // Grab command-line arguments.
    let args: Vec<String> = env::args().collect();
    if args.len() < 3 || args.len() > 5 {
        return Err(format!(
            "Usage: cargo run -- <svg> <dimensions> [<min_x,min_y,max_x,max_y>] [<out_fp>]"
        ));
    }

    // Ensure <in-svg> is an actual SVG file.
    let svg_fp = &args[1];
    if !svg_fp.ends_with(".svg") {
        return Err(format!("<in-svg> must be an SVG"));
    }

    // Ensure <dimensions> are in the WxH format, and are positive integers.
    let dims: Vec<&str> = args[2].split('x').collect();
    if dims.len() != 2 {
        return Err(format!(
            "<dimensions> must be in the format <width>x<height>"
        ));
    }

    let width: i32 = dims[0].trim().parse::<i32>().map_err(
        |e| format!("Invalid width '{}': {}", dims[0].trim(), e)
    )?;
    let height: i32 = dims[1].trim().parse::<i32>().map_err(
        |e| format!("Invalid height '{}': {}", dims[1].trim(), e)
    )?;

    if width <= 0 { return Err(format!("Width must be positive.")); }
    if height <= 0 { return Err(format!("Height must be positive.")); }

    // Extract bbox and/or out_fp if specified.
    let (bbox_arg, out_fp) = match args.len() {
        4 if args[3].contains(',') => (Some(&args[3]), None),
        4 => (None, Some(args[3].clone())),
        5 => (Some(&args[3]), Some(args[4].clone())),
        _ => (None, None),
    };

    let bbox = if let Some(bbox_arg) = bbox_arg {
        let parts: Vec<&str> = bbox_arg.split(',').collect();
        if parts.len() != 4 {
            return Err(format!("<crop> must be min_x,min_y,max_x,max_y"));
        }

        let vals: Result<Vec<f32>, _> = parts.iter().map(
            |p| p.trim().parse::<f32>()
        ).collect();
        let vals = vals.map_err(|e| format!("Invalid crop: {}", e))?;

        Some((vals[0], vals[1], vals[2], vals[3]))
    } else {
        None
    };

    // Args validated; return them.
    Ok(UserInput {
        svg_fp: svg_fp.to_string(),
        constraint_width: width,
        constraint_height: height,
        crop: bbox,
        out_fp
    })
}

fn main() -> Result<(), Box<dyn Error>> {
    // Collect command-line arguments.
    let user_input = collect_args()?;
    println!(
        "Calculating scalar to fit SVG w/in {}x{}...\r\n",
        user_input.constraint_width,
        user_input.constraint_height
    );

    // Create SVG tree using usvg.
    let mut opt = usvg::Options { ..usvg::Options::default() };
    opt.resources_dir = fs::canonicalize(&user_input.svg_fp).ok().and_then(
        |p| p.parent().map(|p| p.to_path_buf())
    );

    let svg_data = fs::read(&user_input.svg_fp).unwrap();
    let tree = usvg::Tree::from_data(&svg_data, &opt).unwrap();

    // If crop is specified, use that.
    // Otherwise, determine scaling factor to fit w/in smaller of dimensions.
    let (min_x, min_y, max_x, max_y) = if let Some(crop) = user_input.crop {
        crop
    } else {
        let mut bboxes = Vec::new();
        collect_bboxes(tree.root(), &mut bboxes);

        let mut min_x: f32 = f32::MAX; let mut max_x: f32 = 0.0;
        let mut min_y: f32 = f32::MAX; let mut max_y: f32 = 0.0;
        for bbox in &bboxes {
            min_x = min_x.min(bbox.left());
            min_y = min_y.min(bbox.top());
            max_x = max_x.max(bbox.right());
            max_y = max_y.max(bbox.bottom());
        }

        (min_x, min_y, max_x, max_y)
    };

    let svg_width = max_x - min_x;
    let svg_height = max_y - min_y;

    let alpha_w = user_input.constraint_width as f32 / svg_width;
    let alpha_h = user_input.constraint_height as f32 / svg_height;
    let alpha = alpha_w.min(alpha_h);

    // Create PNG from scaled SVG:
    //    - SVG will be centered horizontally and vertically w/in PNG.
    //    - PNG's transparent background will be removed, and the SVG's path's
    // will be in black, instead of white (for drawing on a black background).
    let canvas_w = user_input.constraint_width as u32;
    let canvas_h = user_input.constraint_height as u32;

    let mut pixmap = 
        ts::Pixmap::new(canvas_w, canvas_h)
        .ok_or("Failed to create Pixmap :(")?;

    let content_w = svg_width * alpha;
    let content_h = svg_height * alpha;
    let offset_x = (canvas_w as f32 - content_w) / 2.0;
    let offset_y = (canvas_h as f32 - content_h) / 2.0;
    let transform = 
        ts::Transform::from_translate(-min_x, -min_y)
        .post_scale(alpha, alpha)
        .post_translate(offset_x, offset_y);

    resvg::render(&tree, transform, &mut pixmap.as_mut());
    for pixel in pixmap.pixels_mut() {
        let a = pixel.alpha();
        *pixel = ts::PremultipliedColorU8::from_rgba(a, a, a, 255).unwrap();
    }

    // Save PNG to a file, and exit succesfully.
    let updated_fp = match &user_input.out_fp {
        Some(fp) => fp.clone(),
        None => user_input.svg_fp.replace(".svg", "_scaled.png"),
    };
    pixmap.save_png(&updated_fp).expect("Failed to save PNG :(");
    println!("Saved scaled SVG as PNG to {}!", updated_fp);

    Ok(())
}

fn collect_bboxes(
    parent: &usvg::Group,
    bboxes: &mut Vec<usvg::Rect>,
) {
    for node in parent.children() {
        if let usvg::Node::Group(group) = node {
            collect_bboxes(group, bboxes);
        }

        let bbox = node.abs_bounding_box();
        let stroke_bbox = node.abs_stroke_bounding_box();
        bboxes.push(if bbox != stroke_bbox { stroke_bbox } else { bbox });
    }
}
