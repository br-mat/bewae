# script to calculate a input list of full hours to binary representation
# where each bit indicates a full hour

def get_binary_representation(hours):
    binary_representation = ['0'] * 24  # Create a list of 24 '0' elements
    for hour in hours:
        binary_representation[23 - hour] = '1'  # Set the corresponding index to '1' for each hour
    return ''.join(binary_representation)  # Convert the list to a string and return it

def get_hours_list(decimal):
    binary_representation = bin(decimal)[2:].zfill(24)  # Convert the decimal to a binary string of length 24
    hours_list = []
    for i, bit in enumerate(binary_representation):
        if bit == '1':
            hours_list.append(23 - i)  # Add the corresponding hour to the list if the bit is '1'
    return sorted(hours_list)  # Sort and return the list of hours

def get_decimal_representation(hours):
    binary_representation = get_binary_representation(hours)  # Get the binary representation of the hours
    return int(binary_representation, 2)  # Convert the binary representation to decimal and return it

# Example usage
input_ = [7, 10, 16]  # Example input, a list of hours
#input_ = 123456789  # Example input, a decimal number
if isinstance(input_, list):  # Check if the input is a list
    binary_representation = get_binary_representation(input_)
    print(f"Binary representation: {binary_representation}")

    decimal_representation = get_decimal_representation(input_)
    print(f"Decimal representation: {decimal_representation}")

    hours_list = get_hours_list(decimal_representation)
    print(f"Hours list: {hours_list}")
elif isinstance(input_, int):  # Check if the input is an integer
    hours_list = get_hours_list(input_)
    print(f"Hours list: {hours_list}")
else:
    print(f"Input is not valid!")  # Print an error message if the input is neither a list nor an integer
