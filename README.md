TressFX Hair for Unreal
=============


Overview
----------------------

This open source Unreal Engine fork implements a **modified** version AMD's [TressFX](https://github.com/GPUOpen-Effects/TressFX) hair/fur rendering and simulation technology into Unreal Engine. TressFX makes use of the processing power of high-performance GPUs to do realistic rendering and utilizes compute shaders to physically simulate each individual strand of hair.

We are opening the source to the community to get more people involved and help solve the remaining issues.

### Current Features and Changes from AMDs [original library](https://github.com/GPUOpen-Effects/TressFX):
- Support for up to 16 bone influences per hair
- Morph target support
- New [exporters for Maya and Blender](https://github.com/kostenickj/TressFXExporter)

    * Previously, boneskinning and hair assets were separate files upon exporting from Maya (.tfx, .tfxbone). We have combined them into a single file in json format for easier exporter debugging: .tfxjson. Upon import into the editor they are converted to binary and saved as UAssets. I consider the blender exporter "beta", It hasn't been tested much yet. The Maya exporter should be production ready. Due to the changes in asset format, old .tfx and .tfxbone binary files are not supported.
	* The blender exporter works but is still considered a work in progress.
- Multi Platform (mostly) thanks to unreal's cross compiling

    TressFX _should_ work on any platform that supports SM5 and above. However, this needs confirmation/testing since so far it has only been tested on windows.

- Two render modes for hair which can be set on a per material basis: 

    1. **Opaque**: Opaque mode _should_ support all unreal engine features.
    2. **Order Independent Transparency (OIT)**:
        OIT mode supports most Unreal Engine features, but is currently limited to 4 dynamic lights (same as the forward renderer). Additionally, there are two OIT modes: Shortcut (recommended), and K-Buffer Linked List (experimental, performance heavy). The mode used can be set from a Console Variable. Information on how each mode works can be found [here](https://gpuopen.com/tressfx-3-1/).

- Velocity Rendering
    - To support Temporal Anti Aliasing and more that Unreal needs.
	
- Physics asset integration
	- Capsule collision with up to 10 capsules can be used.
	- Only capsules/sphyls are currently supported.
	- Uses the same physics asset as the underlying mesh. To mark a capsule for TressFX collision, the name of the capsule must end with "tressfx". A bool flag will be added eventually.

Shading Models
--------------------

Currently, we recommend using UE4's built in hair shading model with TressFX. It has nearly full material editor integration and plays nicer with Unreal's systems. There is also an experimental TressFX shading model, ported from AMD's original sample that has _partial_ material editor integration, but many parameters can only be set from the TressFXComponent.

Console Variables
----------------------
All console variables related to TressFX follow the pattern tfx.*

todo

Known Issues/Future Work
----------------------
- Dynamic Shadows technically "work" but improvement is needed.
    - Hair can recieve shadow and cast shadow onto the scene, but self shadow is not quite good enough yet. I plan on implementing Adaptive Volumetric Shadow Maps for self shadowing. I Would love to hear other ideas.
- Raytracing Support, see [here](https://github.com/kostenickj/UE4_TressFX/issues/22).
- Only 4 dynamic lights currently supported at once with OIT (opaque has no limitation). Would love to hear others' ideas on how to increase this to 8.


Want to help?
----------------------
Please do! Pull request are very welcome! There are are many things that can be improved and several features that are not fully implemented and we want help! See the [issues](https://github.com/kostenickj/UE4_TressFX/issues) on github for starters. 

Please follow the Unreal Engine [Coding standards](https://docs.unrealengine.com/en-US/Programming/Development/CodingStandard/index.html).

Join the TressFX for Unreal discord server to coordinate work and discuss ideas: https://discord.gg/aM5Ge5Y

Bugs? Yes. When you encounter bugs please open an [issue](https://github.com/kostenickj/UE4_TressFX/issues).

Branches
----------------------
Branches that end with "Main" are generally the stable branches: We try not to push anything there that will crash. Example: 423TFXMain.

The main dev branch is \<engine version>Dev. We stage changes here from individual author branches for testing before merging them to Main. Make pull requests against this branch please.


FAQ
----------------------
### Does this use the forward renderer?

No. It uses some of the same systems and concepts as the Forward Renderer, but it is _not_ compatible with forward rendering. The lighting system is comparable to the "Translucency Surface Forward Shading" lighting mode.


### What about VR or instanced stereo?

Untested, but I probably will not work without changes. Its not something i personally plan on adding, but pull requests are certainly welcome :)

### Raytracing?

Not _yet_.


---
---
---

Unreal Engine
=============

Welcome to the Unreal Engine source code! 

From this repository you can build the Unreal Editor for Windows, Mac and Linux, compile Unreal Engine games for Android, iOS, PlayStation 4, Xbox One and HTML5,
and build tools like Unreal Lightmass and Unreal Frontend. Modify them in any way you can imagine, and share your changes with others! 

We have a heap of documentation available for the engine on the web. If you're looking for the answer to something, you may want to start here: 

* [Unreal Engine Programming Guide](https://docs.unrealengine.com/latest/INT/Programming/index.html)
* [Unreal Engine API Reference](https://docs.unrealengine.com/latest/INT/API/index.html)
* [Engine source and GitHub on the Unreal Engine forums](https://forums.unrealengine.com/forumdisplay.php?1-Development-Discussion)

If you need more, just ask! A lot of Epic developers hang out on the [forums](https://forums.unrealengine.com/) or [AnswerHub](https://answers.unrealengine.com/), 
and we're proud to be part of a well-meaning, friendly and welcoming community of thousands. 


Branches
--------

We publish source for the engine in several branches:

The **[release branch](https://github.com/EpicGames/UnrealEngine/tree/release)** is extensively tested by our QA team and makes a great starting point for learning the engine or
making your own games. We work hard to make releases stable and reliable, and aim to publish new releases every few months.

The **[promoted branch](https://github.com/EpicGames/UnrealEngine/tree/promoted)** is updated with builds for our artists and designers to use. We try to update with merges from the master branch daily (though we often catch things that prevent us from doing so) and it's a good balance between getting the latest cool stuff and knowing most things work.

The **[master branch](https://github.com/EpicGames/UnrealEngine/tree/master)** is the hub of changes from all our specialized engine development teams. Our internal game teams typically take engine snapshots from here, but it isn't subject to as much testing as release branches.

Individual teams have their own **development branches** for day to day work ([dev-core](https://github.com/EpicGames/UnrealEngine/tree/dev-core), [dev-mobile](https://github.com/EpicGames/UnrealEngine/tree/dev-mobile) and [dev-sequencer](https://github.com/EpicGames/UnrealEngine/tree/dev-sequencer), for example). These branches reflect the cutting edge of the engine and may be buggy - they may not even compile. Battle-hardened developers eager to test new features or work lock-step with us should head to one of these. We aim to merge development branches to master every 3-4 weeks.

Other short-lived branches may pop-up from time to time as we stabilize new releases or hotfixes.


Getting up and running
----------------------

The steps below will take you through cloning your own private fork, then compiling and running the editor yourself:

### Windows

1. Install **[GitHub for Windows](https://windows.github.com/)** then **[fork and clone our repository](https://guides.github.com/activities/forking/)**. 
   To use Git from the command line, see the [Setting up Git](https://help.github.com/articles/set-up-git/) and [Fork a Repo](https://help.github.com/articles/fork-a-repo/) articles.

   If you'd prefer not to use Git, you can get the source with the 'Download ZIP' button on the right. The built-in Windows zip utility will mark the contents of zip files 
   downloaded from the Internet as unsafe to execute, so right-click the zip file and select 'Properties...' and 'Unblock' before decompressing it. Third-party zip utilities don't normally do this.

1. Install **Visual Studio 2017**. 
   All desktop editions of Visual Studio 2017 can build UE4, including [Visual Studio Community 2017](http://www.visualstudio.com/products/visual-studio-community-vs), which is free for small teams and individual developers.
   To install the correct components for UE4 development, check the "Game Development with C++" workload, and the "Unreal Engine Installer" and "Nuget Package Manager" optional components.
  
1. Open your source folder in Explorer and run **Setup.bat**. 
   This will download binary content for the engine, as well as installing prerequisites and setting up Unreal file associations. 
   On Windows 8, a warning from SmartScreen may appear.  Click "More info", then "Run anyway" to continue.
   
   A clean download of the engine binaries is currently 3-4gb, which may take some time to complete.
   Subsequent checkouts only require incremental downloads and will be much quicker.
 
1. Run **GenerateProjectFiles.bat** to create project files for the engine. It should take less than a minute to complete.  

1. Load the project into Visual Studio by double-clicking on the **UE4.sln** file. Set your solution configuration to **Development Editor** and your solution
   platform to **Win64**, then right click on the **UE4** target and select **Build**. It may take anywhere between 10 and 40 minutes to finish compiling, depending on your system specs.

1. After compiling finishes, you can load the editor from Visual Studio by setting your startup project to **UE4** and pressing **F5** to debug.




### Mac
   
1. Install **[GitHub for Mac](https://mac.github.com/)** then **[fork and clone our repository](https://guides.github.com/activities/forking/)**. 
   To use Git from the Terminal, see the [Setting up Git](https://help.github.com/articles/set-up-git/) and [Fork a Repo](https://help.github.com/articles/fork-a-repo/) articles.
   If you'd rather not use Git, use the 'Download ZIP' button on the right to get the source directly.

1. Install the latest version of [Xcode](https://itunes.apple.com/us/app/xcode/id497799835).

1. Open your source folder in Finder and double-click on **Setup.command** to download binary content for the engine. You can close the Terminal window afterwards.

   If you downloaded the source as a .zip file, you may see a warning about it being from an unidentified developer (because .zip files on GitHub aren't digitally signed).
   To work around it, right-click on Setup.command, select Open, then click the Open button.

1. In the same folder, double-click **GenerateProjectFiles.command**.  It should take less than a minute to complete.  

1. Load the project into Xcode by double-clicking on the **UE4.xcworkspace** file. Select the **ShaderCompileWorker** for **My Mac** target in the title bar,
   then select the 'Product > Build' menu item. When Xcode finishes building, do the same for the **UE4** for **My Mac** target. Compiling may take anywhere between 15 and 40 minutes, depending on your system specs.
   
1. After compiling finishes, select the 'Product > Run' menu item to load the editor.




### Linux

1. [Set up Git](https://help.github.com/articles/set-up-git/) and [fork our repository](https://help.github.com/articles/fork-a-repo/).
   If you'd prefer not to use Git, use the 'Download ZIP' button on the right to get the source as a zip file.

1. Open your source folder and run **Setup.sh** to download binary content for the engine.

1. Both cross-compiling and native builds are supported. 

   **Cross-compiling** is handy when you are a Windows (Mac support planned too) developer who wants to package your game for Linux with minimal hassle, and it requires a [cross-compiler toolchain](http://cdn.unrealengine.com/CrossToolchain_Linux/v11_clang-5.0.0-centos7.zip) to be installed (see the [Linux cross-compiling page on the wiki](https://docs.unrealengine.com/latest/INT/Platforms/Linux/GettingStarted/)).

   **Native compilation** is discussed in [a separate README](Engine/Build/BatchFiles/Linux/README.md) and [community wiki page](https://wiki.unrealengine.com/Building_On_Linux). 




### Additional target platforms

**Android** support will be downloaded by the setup script if you have the Android NDK installed. See the [Android getting started guide](https://docs.unrealengine.com/latest/INT/Platforms/Android/GettingStarted/).

**iOS** programming requires a Mac. Instructions are in the [iOS getting started guide](https://docs.unrealengine.com/latest/INT/Platforms/iOS/GettingStarted/index.html).

**HTML5** support will be downloaded by the setup script if you have Emscripten installed. Please see the [HTML5 getting started guide](https://docs.unrealengine.com/latest/INT/Platforms/HTML5/GettingStarted/index.html).

**PlayStation 4** or **XboxOne** development require additional files that can only be provided after your registered developer status is confirmed by Sony or Microsoft. See [the announcement blog post](https://www.unrealengine.com/blog/playstation-4-and-xbox-one-now-supported) for more information.


Licensing and Contributions
---------------------------

Your access to and use of Unreal Engine on GitHub is governed by the [Unreal Engine End User License Agreement](https://www.unrealengine.com/eula). If you don't agree to those terms, as amended from time to time, you are not permitted to access or use Unreal Engine.

We welcome any contributions to Unreal Engine development through [pull requests](https://github.com/EpicGames/UnrealEngine/pulls/) on GitHub. Most of our active development is in the **master** branch, so we prefer to take pull requests there (particularly for new features). We try to make sure that all new code adheres to the [Epic coding standards](https://docs.unrealengine.com/latest/INT/Programming/Development/CodingStandard/).  All contributions are governed by the terms of the EULA.


Additional Notes
----------------

The first time you start the editor from a fresh source build, you may experience long load times. 
The engine is optimizing content for your platform to the _derived data cache_, and it should only happen once.

Your private forks of the Unreal Engine code are associated with your GitHub account permissions.
If you unsubscribe or switch GitHub user names, you'll need to re-fork and upload your changes from a local copy. 

