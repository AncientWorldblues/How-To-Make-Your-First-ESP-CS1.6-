## How to Make Your First ESP - CS 1.6

## Introduction

This project is a beginner-friendly to teach you how to create your first ESP tool for Counter-Strike 1.6.

This tutorial is designed for beginner who:

- Are just starting to learn C/C++
- Are just starting to learn game memory.
- Want to understand how the ESP development process worked **step by step**.

In this project, you will learn how to:

- Use Cheat Engine (CE) to find in-game values like player coordinates, health, team, and pointers
- Understand and analyze game memory
- Apply basic C programming techniques to read these values from memory
- Convert 3D world positions to 2D screen positions (WorldToScreen)
- Understand how the Windows overlay works that draws simple ESP boxes
- Understand the structure of a basic external ESP: a memory reader DLL and a separate overlay app

The focus is not on advanced coding, but on **clarity**. The code is kept simple, even if you're not confident in your programming skills, and I try to make everything is explained step-by-step to help you follow along.

## Legal Disclaimer

This project is for **educational and personal learning** purposes only.

You must agree to the following terms:

- Do **not** use this code to cheat in any online multiplayer or competitive game environment.
- This project is **not affiliated** with or endorsed by Valve or any game developer.
- The author is **not responsible** for any misuse of this project or resulting consequences.
- You use or modify this code **at your own risk**.

Always respect the Terms of Service and community rules of any game you interact with.

## Prerequisites

- Cheat Engine: [https://cheatengine.org](https://cheatengine.org)
- Visual Studio Community: [https://visualstudio.microsoft.com/](https://visualstudio.microsoft.com/)
- CS 1.6 game files: find yourself
- Extreme Injector: search online (may be flagged by antivirus)
