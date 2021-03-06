# GGNoRe-CPP-API-IntegrationsTest

Tests for the rollback module GGNoRe.

# The problem

Video games are made of 3 parts looping until you stop playing: poll inputs -> simulate new state -> render view. Online video games have the added constraint that you cannot poll the remote inputs for the current local frame because it takes time for them to travel the network.

# The solutions

- Run the local and remote clients in lockstep, with the local client waiting for the remote inputs before simulating. This delays the game by the network travel time and makes it feel sluggish because what is effectively being rendered is what happened in the past.
- Locally extrapolating then reconciliating the state of a remote player. The caveat is that the state of a player is never exactly the same across different clients which may lead to the different players experiencing different versions of the game and disagreeing on the outcome.
- Predict the remote inputs by assuming that they are the same as the previous frame since it is often the case. Each frame a checksum is generated from the state of the game, so when the local and remote checksums do not match, a mis-prediction occured. In order to resolve a mis-prediction, load a previous valid state of the game, aka rollbacking, and resimulate up to the current frame with the correct inputs. Rollback combines immediate responsiveness with consistency across every client.

# More on rollback

From the best [writeup](https://ki.infil.net/w02-netcode-p4.html) on the topic:

> Rollback's main strength is that it never waits for missing input from the opponent. Instead, rollback netcode continues to run the game normally. All inputs from the local player are processed immediately, as if it was offline. Then, when input from the remote player comes in a few frames later, rollback fixes its mistakes by correcting the past. It does this in such a clever way that the local player may not even notice a large percentage of network instability, and they can play through any remaining instances with confidence that their inputs are always handled consistently.

For more technical resources, check out the GDC talks from [Overwatch](https://youtu.be/W3aieHjyNvw?t=1341) and [Mortal Kombat](https://www.youtube.com/watch?v=7jb0FOcImdg).

# Why use GGNoRe

GGNoRe encourages a structured implementation instead of respectively serializing/simulating everything in a single callback each, as it is usual in other implementations tailored to smaller games or emulators where it is possible to save the state of the entire game for multiple frames. This structure greatly helps with transitioning delay based netcode into rollback as well since you can simply attach components to your already existing architecture and still use your regular simulation loop.

# In this repository

There are only the tests in order to demo the API and features. The module is in a private repository (see Licensing section at the bottom).

# Features

- Highly configurable: delay frames count, input leniency buffer size, rollback buffer size and more.
- Integration tests for demo purposes and manual debugging. The entirety of the module has been automatically tested with close to 250k different configurations.
- Very well documented, namely to offer advice on use and to avoid pitfalls.
- Manages actor lifetime with out of the box support for your game's pooling through activation/deactivation interface.
- Supports both fixed update and updating every tick. For example you could use the ticking callback to update animation playing/player location instead of every fixed logic frame. Still, the async tick logic is locked back with the core clock when a fixed logic frame is completed, so animations/locations would match with the actual state of the game.
- Possibility to periodically stall players with frame advantage, mitigating one sided rollback.
- Possibility to periodically skip rendering frames, when punctually GPU bottlenecked for example.
- Optimized bit buffer for the inputs packet.
- Built-in input transfer protocol.
- Synchronizing with an ongoing game session.
- Supports any number of local/remote players.
- Resistant to some types of cheats. At runtime a server could decide what is simulated locally, using a significance manager for example, so a client would not have information that could be misused. And memory tempering cheats would desync the client since the memory content must be identical.
- Although the module uses the STL containers with the default allocator, the heap memory needs are bounded by the configuration so there are no unnecessary allocations. The state serialization/deserialization is the only memory sensitive operation but in this case you may use the optimized containers coming with your engine.
- Beside the STL everything is tailor-made for the module, there are no 3rd party libraries.
- Detailed logs.
- [Offensive programming](https://en.wikipedia.org/wiki/Offensive_programming) style so no bugs go through unnoticed.
- Lean API, only what you need to use is exposed.
- Internally uses fixed point to ensure serialization consistency.
- Can force rollback in order to test if your game still works in the worst case scenario.
- Mock remote players locally.
- Static library but can easily be turned into a dynamic one.

# Caveats

- Your game must be fully deterministic, meaning that for given inputs and initial conditions, the simulation outputs the exact same result. This may be impossible for games with complex physics.
- Your game simulation must be performant enough to run multiple times per frame.

# Building

## Windows

There is a sln for the VS2019 toolchain.

## Others

Since there is very little platform specific code, all contained within `GGNoRe-CPP-API/Core/Build.hpp`, and since I exclusively use standard C++14, it should be cross-platform as well as any compiler friendly.

# Buffer structure example

This is an example, the actual values depend on how you configure the module.

<!---
graphviz
digraph G {
  { 
    node [shape=plaintext fontcolor=silver]
    
    buffer [label=<<TABLE BORDER="0" CELLBORDER="1" CELLSPACING="0">
    <TR>
        <td BGCOLOR="blue" WIDTH="50" HEIGHT="50"></td>
        <td BGCOLOR="blue" WIDTH="50" HEIGHT="50"></td>
        <td BGCOLOR="blue" WIDTH="50" HEIGHT="50"></td>
        <td BGCOLOR="blue" WIDTH="50" HEIGHT="50"></td>
        <td BGCOLOR="blue" WIDTH="50" HEIGHT="50"></td>
        <td BGCOLOR="blue" WIDTH="50" HEIGHT="50"></td>
        <td BGCOLOR="blue" WIDTH="50" HEIGHT="50"></td>
        <td BGCOLOR="fuchsia" WIDTH="50" HEIGHT="50"></td>
        <td BGCOLOR="fuchsia" WIDTH="50" HEIGHT="50"></td>
        <td BGCOLOR="red" WIDTH="50" HEIGHT="50"></td>
        <td BGCOLOR="yellow" WIDTH="50" HEIGHT="50"></td>
        <td BGCOLOR="yellow" WIDTH="50" HEIGHT="50"></td>
    </TR>
    </TABLE>>];
    
    legend [label=<<TABLE BORDER="0" CELLBORDER="0" CELLSPACING="0">
    <TR><td BGCOLOR="blue">RollbackBufferMaxSize = 7</td></TR>
    <TR><td BGCOLOR="fuchsia">InputLeniencyFramesCount = 2</td></TR>
    <TR><td BGCOLOR="red">Current tick frame</td></TR>
    <TR><td BGCOLOR="yellow">DelayFramesCount = 2</td></TR>
    </TABLE>>];
  }
}
--->

![Buffer structure](./buffer_structure.svg)

`EmulatorInputBufferSize = InputLeniencyFramesCount + Current tick frame`

- rollback is possible up to `Current tick frame - RollbackBufferMaxSize`.
- generating an input at `Current tick frame` saves it to `Current tick frame + DelayFramesCount`.

# Logic overview

- some details, such as how the delay prevents rollbacking, are omitted for clarity.
- "unsimulated" means that the change in activation for a component happened outside of the implementation of its simulator interface, namely `OnSimulateFrame`/`OnSimulateTick`.

## Simulation

Configured with a logic frame time of 16.6ms.

|Frame|Instruction|Delta time accumulator|
|-|-|-|
|0|Initialize state|0.0|
|0|Serialize state|0.0|
|0|Tick(7.0)|7.0|
|0|Unsimulated|7.0|
|0|Tick(16.6 - 7.0 = 9.6)|19.0|
|0|Simulate frame(16.6)|19.0|
|1|Unsimulated|2.4|
|1|Serialize state|2.4|

## Rollback

|Frame|Instruction|
|-|-|
|1|Receive remote inputs or a change in activation for frame 0|
|1|Revert activation changes from simulating frame 1|
|1|Revert activation changes from ticking frame 1|
|1|Revert unsimulated activation changes of frame 1|
|1|Revert activation changes from simulating frame 0|
|1|Revert activation changes from ticking frame 0|
|1|Deserialize state of frame 0|
|0|Resimulate|

# TBD

- Visual debug inspector.
- Unreal Engine 4/5 plugins.
- Option to keep simulating even if starved for inputs. That way it would be possible to combine rollback with repairing the state like using regular interpolation netcode. It could also be used to combine rollback for highly relevant players and a more lenient solution for the remaining ones.
- At the moment every player of a session must use the same frame buffer configuration (delay, leniency...). In the future, it might be configurable per player.
- Variable rollback buffer size.

# Licensing

Contact me: contact@erebnyx.com