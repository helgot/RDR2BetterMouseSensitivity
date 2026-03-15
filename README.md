# Description
Allows the user to freely mouse sensitivity in video game Red Dead Redemption 2 for PC (Steam).

[Nexus Page](https://www.nexusmods.com/reddeadredemption2/mods/5463)

# Building

This project can be built using **CMake** and the **Vulkan SDK**

---

## Prerequisites

Ensure the following tools are installed:

- [CMake](https://cmake.org/download/)
- [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)

---

## Build Instructions (Windows)

### 1. Clone the Repository
```powershell
git clone --recurse-submodules https://github.com/helgot/RDR2BetterMouseSensitivity.git
cd RDR2BetterMouseSensitivity
```
### 3. Configure 
```
cmake --preset win-amd64-relwithdebinfo
```
### 4. Build
```
cmake --build out\build\win-amd64-relwithdebinfo --target RDR2BetterMouseSensitivity
```
RDR2BetterMouseSensitivity.asi will be built in the out\build\win-amd64-relwithdebinfo directory.

### Remarks
If submodules are missing, rerun:
```
git submodule update --init --recursive
```

# Credits
[ImGui](https://github.com/ocornut/imgui)
[MinHook](https://github.com/TsudaKageyu/minhook)

