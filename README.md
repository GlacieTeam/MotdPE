# MotdPE
Minecraft MotdPE Library

## Usage
```C++
// Sync
std::string motdpe::queryMotd("example.com", 19132, std::chrono::seconds(5));

// Async
std::future<std::string> motdpe::queryMotdAsync("example.com", 19132, std::chrono::seconds(5));
```

## Install
```xmake
add_repositories("groupmountain-repo https://github.com/GroupMountain/xmake-repo.git")

add_requires("binarystream")
```

## Build
- Build with Xmake
```bash
xmake --all
```
- If you want to use Cmake build system, you can generate CmakeLists.txt
```bash
xmake project -k cmake
```

## License
This project is licensed under the **Mozilla Public License 2.0 (MPL-2.0)**.  

### Key Requirements:
- **Modifications to this project's files** must be released under MPL-2.0.  
- **Static/dynamic linking with closed-source projects** is allowed (no requirement to disclose your own code).  
- **Patent protection** is explicitly granted to all users.  

For the full license text, see [LICENSE](LICENSE) file or visit [MPL 2.0 Official Page](https://www.mozilla.org/en-US/MPL/2.0/).  

---

### Copyright © 2025 GlacieTeam. All rights reserved.