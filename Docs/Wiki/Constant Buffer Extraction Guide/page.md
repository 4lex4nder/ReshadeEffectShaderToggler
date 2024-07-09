# Constant Buffer Extraction with ReShade Effect Shader Toggler

ReShade Effect Shader Toggler (REST) offers several features for analyzing and responding to game data, the most unintuitive of which being Constant Buffer extraction. I had difficulty understanding how to use this feature at first and while I'm still no expert, I was able to create a clear working sample that demonstrates its usage. This is not a comprehensive guide on the constant buffer itself but you can fill in the remaining info by studying your graphics API of choice. If you have additional information or any corrections please feel free to raise an issue or fork the repo and directly edit.

REST reads game shaders and lists them in descending order as they occurred over the sample duration, measured in frames that can be specified in the addon options. The initial value will likely be sufficient for things like UI but consider using a longer duration if searching deeper for game information. The primary use case is to control when and where to apply ReShade effects but it can also copy render targets from the game to textures and extract constant buffer. In fact you can do all 3 in a single group though this is probably not wise due to performance concerns (CB extraction is heavier than the other options) and because it goes against the general intent of this addon which is meant to use multiple groups. 

![REST](Images/REST)


Continuing with the options, Resource Shim allows for filtering only srgb resources which maybe necessary in some games. If you know you need srgb only perhaps this might serve as an optimization but the safer option is to leave it at none. There is also an option for FF14 that seems to rely on datamined game information to enable and perhaps hasten resource collection. If you have similar info consider forking and rebuilding the addon to utilize it. For the rest of us just leave it at none and move on.    

To use Constant Buffer values you must enable a CB copy method as well as "Track descriptors" and the group itself. I recommend not enabling "prevent runtime reload" as this can lead to massive drops in performance, though it might be possible to mitigate with the right settings. If you're getting constant reloads I would rethink what you're trying to do. Likely the shader you're binding is not a great choice. I couldn't find a useful shader to bind to that didn't drop me to 30 FPS or less when using this option.

I have found success using gpu_readback. FF14 and nier replicant have dedicated options here that I assume would be pointless for other games so the other valid options are memcpy_singular and memcpy_nested. All of these methods rely on the ReShade API to get resource data so refer there for more detail but from what I know memcpy will try to get the data on upload from cpu to gpu. Its never worked for me in any game so I can't say anything more at this time. Importantly, this setting must be set in the ini before startup or you have to restart after setting it.

One very important thing to note is that in my experience even if set to be active at startup, the CB data isn't copied until I've opened the shader list once and selected the desired shader. You can single click a shader and get the CB but this doesn't save in the ini unless you double click to mark it which can also activate the effects tab functions. You can set to a high index render target to avoid issues here or do multiple things with one group. This could easily be game specific or perhaps there's a way to ensure it happens automatically. If not you should probably consider forking the repo and adding that feature if you intend to share your shader and config.

Its probably also important to touch on what values the constant buffer typically contains. Most values you'll find correspond to the view matrix, projection matrix, view projection matrix, or their inverses (traditionally I've heard model, view, projection). These 4x4 matrices hold sets of transformations (i.e. translation and rotation) that map the coordinates of vertices from local space to world space, world space to camera space, or camera space to screen space. Typically shaders work in camera space therefore when working on world-oriented vertices we can multiply the coordinates by the view matrix which contains the inverse transformations for the camera or in other words it tells the renderer how to quickly get from the object to the camera to simplify math. If you've used color space transformations the concept is pretty much the same.

Other important variables include camera/eye position - a 3 member vector that can be found in the last column or row of a view matrix, Far/Near - singular floats also found in the matrices, timers - also singular floats, and previous frame versions of any of the matrices. 

When browsing through constant buffers you will see many of these matrices and variables though its likely to be unclear which correspond to which and some games may store unrelated info in the CB, for instance its likely you can find your screen resolution and max framerate. Many values will change as you move your camera or character while others will change on their own. These could be timers or other random info the developers wanted to store.

We don't actually necessarily need to use any of these matrices or variables in ReShade effects or REST since ReShade gives us screen space coordinates and depth, but all of the previously mentioned variables are available for binding in REST. However, these variables are simply named according to the above info and can actually be used as accessors for anything you want. The clear exceptions are the prev and inv versions of each matrix which do in fact work as expected. There is no support for custom named variables, although you could perhaps add them in the ini Constants section. While I don't think it would be wise to have many groups relying on different constant buffers, I would think it only requires one GPU readback so it shouldn't add too much latency, however you can use the given variable bindings which seems likely to limit the use cases. 

Before proceeding with my example the last feature to mention is the option to push to the CB rather than reading from the buffer. This may need to be used with memcpy since we would be uploading from cpu to gpu. I don't have a use case for it so I couldn't test it. Presumably you could do things like make your own timer but I don't see any reason why I would want to write to the CB when ReShade has other options for storing and accessing data 
 
I will be looking at constant buffer readings in a certain anime game running on D3D11. This game happens to store the computed value of the healthbar as a float in a constant buffer. Knowing this ahead of time (accessed in 3dmigoto mods with the line `store = $health, ps-cb0, 33`) I quickly found the appropriate shaders at offset 0x080 by 0x04 which comes out to 0x084 or 132. Note that while I have highlighted the cells for easy viewing purpose there is no feature to do this in REST.

![High HP](Images/hihp)

For those not keen on quick maths I can confirm for you that 18657 / 23111 does indeed equal 0.80730391 and 550/20626 does equal 0.02666509

![Low HP](Images/lowhp)

You will likely not find it as easy to get useful data unless you know what you're looking for ahead of time. An addon that should be helpful is [renodx](https://github.com/clshortfuse/renodx). I am still exploring its usage myself but it allows for live disassembly of shaders using the [3dmigoto HLSL decompiler](https://github.com/bo3b/3Dmigoto), while outputting shader names in the same format as ReShade. Note that if using renodx with certain games including the one in this example you should remove any dumped shaders from the game folder on startup as it can cause the game to fail to verify some data.

Back to our example, the health storage can be found in the first vertex shader and on occasion in multiple early pixel shaders, though I settled on binding one from each stage and setting the CB stage to pixel. It appears that even if you select a vertex shader from the list, setting the CB stage to pixel will find the corresponding pixel shaders and vice versa but I don't have full clarity on this yet. 

To be honest getting the health value is not very useful but it does make for a very simple to understand sample that demonstrates how to use the feature without getting into matrix multiplication. When switching character the CB is fairly slow to update so this can't be used for anything requiring very low latency, but I made a quick and dirty shader that adds a red tint at low health. I also found a few timers that continually ticked up in a much later shader stage which I bound to a timer variable in a second shader effect group and used to make a strobing effect. If I'm going to make a completely useless feature I'm going to at least make it flashy.

I found the health value stored early in the pipeline in slot 2 binding 0 at 0x080 by 0x04 or offset 0x084. This is equal to 132 in base 10 (0x080 = 128) although this is not actually relevant to the ways we will interact with the graphics API here. It is stored in base10 in the ini though. 

To use this reading in a shader we need to add variable bindings. I decided to use the Near value to store my health reading and used a prevviewmatrix to get the old values, thus allowing me to calculate damage or account for sudden large cases when switching character or opening menus that unset the value (setting to 0). To actually get these values I added a variable binding at 0x084 for near and chose to retain previous value. I'm not sure exactly what this option does as both my damage calc and straight health readings seem to work no matter if I select it or not. Possibly only comes into play when using push mode or another feature of REST not covered here. Lastly I set a timer binding in another shader group as mentioned and we're ready to write our shader.

![varbinding](Images/varbinding)

These bindings can be accessed by assigning them to uniforms with the "source" annotation provided in ReShade's fx language. For example here is a "ViewMatrix" variable which produces a 4x4 array: 
```
uniform float4x4 ViewMatrix < source = "ViewMatrix"; >;
```
Its important to note once again that these bindings do not actually have any data associated with their names. Its just for your own reference. As an aside I found that texture copy has the same sort of thing going on with the option to input a semantic that doesn't actually actual query or filter by semantic.

Back to the "viewmatrix", components can be read like so:
```
float x = ViewMatrix[0][0]
```
To get my previous health value I set a PrevViewMatrix binding to 0x084 and used `PrevViewMatrix[0][0]`. If you've used arrays in any C derivative language you should understand where to go from here but please refer to the HLSL docs for more specific info and consider that REST presents and reads data in a column-major format but some games may store data in row-major format instead.

I got the other variables in my shader with the following lines: 
```
uniform float Health < source = "Near"; >;
uniform float Timer < source = "Timer"; >;
```
Next I made a new technique rendering on the PostProcessVS shader with a single custom pixel pass. I grabbed the input color and set up a lerp between the original color and a red tinted color. Although optimization here is not really a major concern I chose to reduce usage of ternary operators and conditionals using an arithmetic approach (this honestly might compile to longer bytecode than using a conditional but it feels cooler, let me cook):

```
float when_lt(float x, float y) {
     return max(sign(y - x), 0.0f);
 } 
```

I used the following code to apply the red tint with a small degree of inverse scaling when below a threshold number. I chose 0.53 because that's the damage my most damaged character was at when I was writing this guide. Realistically you would put this to 0.15 or 0.2 if you actually wanted this feature:

```
color.r = when_lt(Health, 0.53f) *( 8.0f - Health);
float fStrength = when_lt(Health, 0.53f) * (0.8f - Health);
```

Damage was handled by the following code. I doubled don with the arithmetic decision making by multiplying the added saturation by another when_lt which would evaluate to 0 when false thus working the same as an if statement. If we were trying to optimize, the previous if statement would still be fine since it returns early and does not start a branch:
```
float Damage = PrevViewMatrix[0][0] - Health;
if (Damage >= 0.4f) return input;
else _saturation += 0.2f * when_lt(0.1f, Damage)
```

As previously mentioned I setup a Timer solely for examples sake, and this setup worked perfectly as expected, disabling the tint periodically when at low health while not doing anything above threshold
```
if (uint(Timer) % 2 == 0) fStrength = 0;
```
Here you can see it in action




https://github.com/elbadcode/ReshadeEffectShaderToggler/assets/10409982/85e91b04-028d-4672-ae60-02811392d001




And see that it does appropriately cease the effect in menus whereas previous attempt were often laggy or failed to clear the value. You will likely need to add similar logical steps to handle whatever CB data you decide to use.




https://github.com/elbadcode/ReshadeEffectShaderToggler/assets/10409982/2a58b06e-9e48-44c8-b524-3a01eccd3fcb




And that's about all I have for now. Hopefully you learned something useful. I've been doing graphics programming for less than a year (directx for only a few months) so I'm no expert and likely won't be able to answer any questions. Most challenges here will be game specific anyway, but I think this should be helpful for providing a concrete and intuitively visualized example of how to use the feature. 


\- Written by [lobotomyx](https://github.com/elbadcode) 
\- [https://github.com/crosire/reshade](ReShade) by Crosire, [ReShade Effect Shader Toggler](https://github.com/4lex4nder/ReshadeEffectShaderToggler) by 4lex4nder, [ReShade Shader Toggler](https://github.com/FransBouma/ShaderToggler) by FransBouma

