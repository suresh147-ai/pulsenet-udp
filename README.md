# pulsenet-udp 🚀

![Version](https://img.shields.io/badge/version-1.0.0-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Build Status](https://img.shields.io/badge/build-passing-brightgreen)

Welcome to the **pulse::net::udp** repository! This project provides raw non-blocking UDP sockets with Go-style ergonomics, implemented in modern C++23. This repository serves as a mirror for easy access and collaboration.

## Table of Contents

- [Features](#features)
- [Installation](#installation)
- [Usage](#usage)
- [Examples](#examples)
- [Contributing](#contributing)
- [License](#license)
- [Releases](#releases)
- [Contact](#contact)

## Features

- **Asynchronous Communication**: Designed for high-performance networking, enabling non-blocking operations.
- **Clean API**: User-friendly interface that simplifies socket programming.
- **Cross-Platform**: Works seamlessly across various operating systems.
- **No Dependencies**: Minimal setup required; no external libraries needed.
- **Modern C++23**: Utilizes the latest C++ features for enhanced performance and readability.
- **Error Handling**: Implements `std::expected` for clear and concise error management.

## Installation

To get started with **pulsenet-udp**, follow these steps:

1. Clone the repository:

   ```bash
   git clone https://github.com/suresh147-ai/pulsenet-udp.git
   cd pulsenet-udp
   ```

2. Build the project using CMake:

   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

3. Install the library:

   ```bash
   sudo make install
   ```

## Usage

Here’s a quick overview of how to use **pulsenet-udp** in your projects.

### Creating a UDP Socket

You can create a UDP socket using the provided API. Here's a simple example:

```cpp
#include <pulse/net/udp/socket.h>

int main() {
    pulse::net::udp::Socket socket;
    socket.bind(12345); // Bind to port 12345

    // Further socket operations...
}
```

### Sending Data

To send data, use the `send` method:

```cpp
socket.send("Hello, World!", "127.0.0.1", 12345);
```

### Receiving Data

Receiving data is just as straightforward:

```cpp
std::string message;
socket.receive(message);
std::cout << "Received: " << message << std::endl;
```

## Examples

You can find detailed examples in the `examples` directory. Each example demonstrates different features of the library, such as:

- Simple UDP client-server communication
- Handling multiple connections
- Error handling using `std::expected`

## Contributing

We welcome contributions! If you would like to contribute to **pulsenet-udp**, please follow these steps:

1. Fork the repository.
2. Create a new branch (`git checkout -b feature-branch`).
3. Make your changes and commit them (`git commit -m 'Add new feature'`).
4. Push to the branch (`git push origin feature-branch`).
5. Create a pull request.

Please ensure that your code follows the existing style and includes appropriate tests.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Releases

For the latest releases, visit our [Releases page](https://github.com/suresh147-ai/pulsenet-udp/releases). Here, you can download and execute the latest versions of the library.

## Contact

For questions or feedback, feel free to reach out:

- **Email**: [your-email@example.com](mailto:your-email@example.com)
- **GitHub**: [suresh147-ai](https://github.com/suresh147-ai)

We appreciate your interest in **pulsenet-udp** and look forward to your contributions!