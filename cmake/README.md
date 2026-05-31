# Viral P2P Volunteer Computing Framework - CMake Build Configuration

## Project Structure

This project is organized following CMake best practices:

```
viral/
├── CMakeLists.txt          # Root CMake configuration
├── src/                    # Source files (.cpp)
│   ├── CMakeLists.txt
│   ├── gbfs.cpp
│   ├── immediate_send.cpp
│   ├── monitor.cpp
│   ├── power_consumption_balancing.cpp
│   ├── result_migration.cpp
│   └── shortest_path.cpp
├── include/                # Header files (.h)
│   ├── gbfs.h
│   └── monitor.h
├── config/                 # Configuration files (XML, etc.)
│   ├── cluster_host.xml
│   ├── cluster_platform.xml
│   ├── platform_file.xml
│   ├── platform_file_zone.xml
│   ├── zones_without_routing.xml
│   ├── large_network.xml
│   ├── large_network_1000.xml
│   ├── large_network_1500.xml
│   └── large_network_alternative_path.xml
├── data/                   # Data files (plots, results, etc.)
├── cmake/                  # Custom CMake modules (if needed)
├── tests/                  # Test files
│   └── CMakeLists.txt
├── docs/                   # Documentation
├── build/                  # Build output (created during build)
├── LICENSE
└── README.md
```

## Building the Project

### Create a build directory:
```bash
mkdir build
cd build
```

### Configure the project:
```bash
cmake ..
```

### Build:
```bash
cmake --build .
```

### Optional: Build with tests:
```bash
cmake -DBUILD_TESTS=ON ..
cmake --build .
ctest
```

### Installation:
```bash
cmake --install .
```

## Key CMake Targets

- `viral_lib` - Main library containing all framework components

## Component Description

- **gbfs**: Greedy Best-First Search implementation
- **immediate_send**: Immediate task sending mechanism
- **monitor**: Network monitoring components
- **power_consumption_balancing**: Load balancing for power consumption
- **result_migration**: Task migration on peer failure
- **shortest_path**: Packet routing using shortest path algorithms
