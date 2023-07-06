def get_binary_representations(hours):
    binary = ''
    for i in range(24):
        if i in hours:
            binary = '1' + binary
        else:
            binary = '0' + binary
    return binary

def get_decimal_representation(hours):
    binary = get_binary_representations(hours)
    decimal = int(binary, 2)
    return decimal

# Example usage
hours_list = [7,19]
binary_representation = get_binary_representations(hours_list)
decimal_representation = get_decimal_representation(hours_list)

print("Binary representation:", binary_representation)
print("Decimal representation:", decimal_representation)
