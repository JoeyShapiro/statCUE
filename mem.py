import numpy as np
import time
import psutil
import os

def monitor_memory_usage():
    """Monitor and display memory usage in MB"""
    process = psutil.Process(os.getpid())
    return process.memory_info().rss / (1024 * 1024)  # Convert to MB

def fill_ram(target_gb=20, step_gb=1, delay_sec=1):
    """
    Fill RAM incrementally up to target_gb
    
    Parameters:
    - target_gb: Total RAM to use in GB
    - step_gb: How much RAM to allocate in each step (GB)
    - delay_sec: Delay between steps in seconds
    """
    print(f"Starting RAM usage: {monitor_memory_usage():.2f} MB")
    print(f"Targeting {target_gb} GB of RAM usage")
    
    # List to keep references to arrays (so they aren't garbage collected)
    arrays = []
    
    # Convert GB to bytes for numpy arrays
    gb_to_bytes = 1024 * 1024 * 1024
    
    current_gb = 0
    while current_gb < target_gb:
        step = min(step_gb, target_gb - current_gb)
        
        # Create a large numpy array (float64 is 8 bytes per element)
        # We divide by 8 to convert from bytes to float64 elements
        array_size = int(step * gb_to_bytes / 8)
        print(f"Allocating {step} GB...")
        
        try:
            # Create and keep a reference to the array
            arrays.append(np.ones(array_size, dtype=np.float64))
            current_gb += step
            
            # Report current usage
            print(f"Current RAM usage: {monitor_memory_usage():.2f} MB ({current_gb} GB allocated)")
            
            # Pause to show ramp-up
            time.sleep(delay_sec)
            
        except MemoryError:
            print(f"Memory error occurred! Could only allocate {current_gb} GB")
            break
    
    print(f"Final RAM usage: {monitor_memory_usage():.2f} MB")
    
    # Keep the arrays in memory until the user decides to release
    input("Press Enter to release memory and exit...")
    
    # Explicitly delete arrays to release memory
    for i in range(len(arrays)):
        arrays[i] = None
    arrays.clear()

if __name__ == "__main__":
    # Specify how much RAM to use (in GB)
    target_ram = 20  # Change this to test different amounts
    step_size = 1    # Increase by 1GB at a time
    delay = 0.5      # Show progress every 0.5 seconds
    
    fill_ram(target_ram, step_size, delay)