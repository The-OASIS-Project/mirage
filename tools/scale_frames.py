#!/usr/bin/env python3
import json
import sys

def scale_value(val, factor):
    """Scale an individual value and round to the nearest integer."""
    return int(round(val * factor))

def scale_dict(data_dict, factor):
    """Scale keys 'x', 'y', 'w', and 'h' in the given dictionary."""
    for key in ['x', 'y', 'w', 'h']:
        if key in data_dict:
            data_dict[key] = scale_value(data_dict[key], factor)
    return data_dict

def main():
    if len(sys.argv) != 3:
        print("Usage: python scale_frames.py input.json output.json")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    scale_factor = 0.5  # The scaling factor you provided

    # Load the JSON file
    with open(input_file, 'r') as f:
        data = json.load(f)

    # Iterate over all frames and scale the relevant numeric values
    for frame_key, frame in data.get("frames", {}).items():
        if "frame" in frame:
            frame["frame"] = scale_dict(frame["frame"], scale_factor)
        if "spriteSourceSize" in frame:
            frame["spriteSourceSize"] = scale_dict(frame["spriteSourceSize"], scale_factor)
        if "sourceSize" in frame:
            frame["sourceSize"] = scale_dict(frame["sourceSize"], scale_factor)

    # Optionally, if you want to update the overall "size" field, uncomment the next lines:
    # if "size" in data:
    #     data["size"] = scale_dict(data["size"], scale_factor)

    # Write the scaled JSON data to the output file
    with open(output_file, 'w') as f:
        json.dump(data, f, indent=4)

    print("Done. Your frames have been scaled.")

if __name__ == '__main__':
    main()

