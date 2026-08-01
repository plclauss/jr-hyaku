import logging, argparse, os.path
import struct, cv2 as cv
from PIL import Image

logger = logging.getLogger(__name__)


def resizeImage(filename: str, resize_to: int, convert: bool) -> bool:
    # Read the image data for resizing.
    original_image = cv.imread(filename)
    if original_image is None:
        logger.error('Failed to read image for resizing')
        return False

    # Resize the image.
    resized_image = cv.resize(original_image, (int(resize_to), int(resize_to)))

    # Write the image to a new file.
    filename_components = os.path.splitext(filename)
    resized_filename = filename_components[0] + f'-{resize_to}x{resize_to}' + filename_components[-1]
    cv.imwrite(resized_filename, resized_image)

    if convert:
        return convertImage(resized_filename)
    return True


def convertImage(filename: str) -> bool:
    # Read the image for converting to RGB565.
    original_image = Image.open(filename)

    # Convert to RGB565.
    original_image = original_image.convert("RGB")
    rgb565_image = bytearray()
    for y in range(original_image.height):
        for x in range(original_image.width):
            r, g, b = original_image.getpixel((x, y))
            grayscale = r >> 3
            rgb565 = ((grayscale << 11) | ((grayscale << 1) << 5) | grayscale)
            rgb565_image += struct.pack(">H", rgb565)
    
    filename_components = os.path.splitext(filename)
    converted_filename = filename_components[0] + f'-RGB565.bin'
    with open(converted_filename, "wb") as f:
        f.write(rgb565_image)

    return True


def main():
    parser = argparse.ArgumentParser(
        prog='image-processing.py',
        description='A tool for resizing images and for converting between image formats'
    )

    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        '-r', '--resize', metavar='DIMENSIONS',
        help='Resize the image to DIMENSIONS (e.g. 128x128). Cannot be used with -c.'
    )
    group.add_argument(
        '-c', '--convert', action='store_true',
        help='Convert the image to RGB565. Cannot be used with -r.'
    )

    parser.add_argument(
        'filename',
        help='Path to the image file to process.'
    )

    args = parser.parse_args()
    
    # Input Validation
    filename = args.filename
    if not os.path.isfile(filename):
        logger.error('Image file must be in the same directory as this script')
        exit(1)
    
    # Handle requested task
    if args.resize:
        if not resizeImage(filename, args.resize, args.convert):
            exit(1)
    elif args.convert:
        if not convertImage(filename):
            exit(1)

    exit(0)


if __name__ == '__main__':
    main()
