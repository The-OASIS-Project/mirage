#!/usr/bin/env python3
import json
import sys

def scale_destinations(obj, scale=0.5):
    """
    Recursively scale numeric values for any key containing 'dest_x' or 'dest_y'
    in dictionaries or lists.
    """
    if isinstance(obj, dict):
        for key, value in obj.items():
            if isinstance(value, (int, float)) and ("dest_x" in key or "dest_y" in key):
                obj[key] = value * scale
            else:
                obj[key] = scale_destinations(value, scale)
    elif isinstance(obj, list):
        for i in range(len(obj)):
            obj[i] = scale_destinations(obj[i], scale)
    return obj

def main():
    if len(sys.argv) != 3:
        print("Usage: python scale_config.py input.json output.json")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    with open(input_file, 'r') as f:
        data = json.load(f)
    
    # Scale all dest_x and dest_y values by 50%
    scaled_data = scale_destinations(data, 0.5)
    
    with open(output_file, 'w') as f:
        json.dump(scaled_data, f, indent=3)
    
    print(f"Scaled all dest_x and dest_y values by 50% and saved to {output_file}")

if __name__ == '__main__':
    main()

