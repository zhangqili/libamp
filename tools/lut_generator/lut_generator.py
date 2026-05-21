# run 'pip install numpy' first
# run 'python lut_generator.py > lut_table.c' save the code
import numpy as np

R = 1.4             # Magnet Radius (mm)
L = 3.15            # Magnet Heignt (mm)

Z_END = 1.6 + 0.3 + 0.6
Z_START = Z_END + 4.0

LUT_LENGTH = 8192

ANALOG_VALUE_MIN = 0
ANALOG_VALUE_MAX = 65535

def calc_b_field_shape(z, r, l):
    z = np.maximum(z, 1e-6) 
    term1 = (l + z) / np.sqrt(r**2 + (l + z)**2)
    term2 = z / np.sqrt(r**2 + z**2)
    return term1 - term2

z_dense = np.linspace(Z_END, Z_START, 2**20)
b_dense = calc_b_field_shape(z_dense, R, L)

b_at_start = calc_b_field_shape(Z_START, R, L)
b_at_end = calc_b_field_shape(Z_END, R, L)

b_normalized = (b_dense - b_at_start) / (b_at_end - b_at_start)

lut_p_keys = np.linspace(0.0, 1.0, LUT_LENGTH)
b_norm_inc = b_normalized[::-1]
z_dense_rev = z_dense[::-1]

lut_z_values = np.interp(lut_p_keys, b_norm_inc, z_dense_rev)

travel_percentage = (Z_START - lut_z_values) / (Z_START - Z_END)

lut_analog_values = ANALOG_VALUE_MIN + travel_percentage * (ANALOG_VALUE_MAX - ANALOG_VALUE_MIN)

min_val_bound = min(ANALOG_VALUE_MIN, ANALOG_VALUE_MAX)
max_val_bound = max(ANALOG_VALUE_MIN, ANALOG_VALUE_MAX)
lut_output = np.clip(np.round(lut_analog_values), min_val_bound, max_val_bound).astype(int)

def generate_c_header():
    normalize_function = '''
AnalogValue advanced_key_normalize(AdvancedKey* advanced_key, AnalogRawValue value)
{
    const uint16_t length = sizeof(table) / sizeof(table[0]);
    int32_t delta = (advanced_key->config.upper_bound - value);
    int16_t index = ((delta * advanced_key->q_scale_to_index) >> 16);
    if (index < 0)
    {
        index = 0;
    }
    if (index >= length)
    {
        index = length - 1;
    }
    return table[index] + ANALOG_VALUE_MIN;
}'''
    print("#include <stdint.h>\n")
    print(f"#if LUT_LENGTH != {LUT_LENGTH}")
    print(f"#warning \"LUT_ENGTH doesn't equal to {LUT_LENGTH}\"")
    print(f"#endif")
    print(f"#ifndef ANALOG_VALUE_MIN")
    print(f"#define ANALOG_VALUE_MIN {ANALOG_VALUE_MIN}")
    print(f"#endif")
    print(f"#ifndef ANALOG_VALUE_MAX")
    print(f"#define ANALOG_VALUE_MAX {ANALOG_VALUE_MAX}")
    print(f"#endif")
    print("")
    
    c_type = "uint16_t"
    if max_val_bound > 65535 or min_val_bound < 0:
        c_type = "int32_t"

    print(f"const {c_type} table[LUT_LENGTH] = {{")
    
    for i in range(0, LUT_LENGTH, 16):
        chunk = lut_output[i:i+16]
        
        chunk_str = ", ".join(f"{val:5d}" for val in chunk)
        if i + 16 < LUT_LENGTH:
            chunk_str += ","
            
        print(f"    {chunk_str:<55} // {i} ~ {min(i+7, LUT_LENGTH-1)}")
        
    print("};")
    
    print(normalize_function)

if __name__ == "__main__":
    generate_c_header()