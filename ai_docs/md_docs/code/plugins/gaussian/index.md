# GaussianSplatting Plugin

       Sorry, your browser does not support embedded videos.
The *GaussianSplatting* plugin allows importing 3D Gaussian Splatting (`*.ply`) files into a UNIGINE application and rendering them in real time.


*3D Gaussian Splatting (3DGS)* primarily uses the *PLY (Polygon File Format)* to store the parameters of millions of Gaussians that define a 3D scene. Because raw *PLY* files are often very large, specialized compressed formats have been developed to reduce file sizes for web and mobile viewing.


**Primary 3DGS File Formats**:


1. **PLY (.ply)**: The standard format for 3D Gaussian Splat data. It stores vertex data, including:

  - Position: 3D coordinates.
  - Scale: Size of the Gaussian along each axis.
  - Rotation: Quaternion for orientation.
  - Opacity: Transparency value.
  - Spherical Harmonics (SH): Color encoding.
2. **SPLAT (.splat)**: A highly efficient, compressed binary format specifically optimized for web-based rendering and real-time viewing.
3. **SPZ (.spz)**: An open-source format developed by Niantic (Scaniverse) designed to act like a "JPG for Gaussian Splatting", offering ~90% compression (10x smaller) compared to standard PLY files, with minimal quality loss.
4. **SOG (Self-Organizing Gaussians)**: Developed by PlayCanvas, this format compresses gaussian PLY files by over 90% by optimizing the data structure.
5. **LCC (.lcc)**: An open-source format by XGRIDS focusing on high-density scene compression and Level-of-Detail (LOD) streaming.
6. **KSPLAT (.ksplat**): A custom, highly compressed format used in *Three.js* implementations for optimized loading.


> **Notice:** Currently, only `.ply` files are supported for Gaussian Splatting assets in UNIGINE. If your data is stored in a different format, you can convert it to `.ply` using third-party tools, for example *[3dgsconverter](https://github.com/francescofugazzi/3dgsconverter)*.


### See Also


- sample in *C++ SIM Samples*


## Launching GaussianSplatting Plugin


To use the *GaussianSplatting* plugin, load it via the `plugin_load` console command or specify the `extern_plugin` command line option on the application start-up:


```bash
-extern_plugin "UnigineGaussianSplatting"
```


## Using GaussianSplatting Plugin


To add a Gaussian Splatting file to the scene, do the following:


1. Open the *Templates* tab in the SDK browser and choose a template that meets your needs. Click *Create Project*. ![](../../../sdk/projects/create_project_cpp.png)
2. In the project creation window that opens, click *Advanced Settings > Plugins*.
3. Enable the `Gaussian Splatting plugin`, click *Add* and *CREATE NEW PROJECT*. The project will appear in the *My Projects* tab list. ![](gaussian_add.png)
4. When the project is created, click the *Customize Unigine Editor Options* button on the projects card, and add the additional argument `-extern_plugin "UnigineGaussianSplatting"`. ![](customize_editor.png)
5. Open UnigineEditor.
6. Create a *Node Dummy* and assign the ***gaussian*** property to it (this property is available in the project as a part of the plugin).
7. In the property parameter ***Gaussian Ply File***, assign the `*.ply` file that you want to render.
8. Adjust other parameters, if necessary.


![](gaussian_prop_settings.png)


### Gaussian Property Parameters


The *Gaussian* property contains the following parameters available for adjustment:


| Gaussian Ply File | The `*.ply` file containing 3D Gaussian splatting [3DGS] scene. |
|---|---|
| Render Order | The order of rendering the 3DGS object. The object with the highest render order will be rendered last (on top of all other 3DGS objects). |
| Render Sequence Order | Specifies at which stage of the rendering pipeline the splats are rendered: - **Transparent** — splats are rendered together with other transparent objects. - **After Tonemapping** — splats are rendered after the tonemapping stage, avoiding tonemapping influence. - **After Post Effects** — splats are rendered after all post-processing effects are applied. |
| Gaussian Render Material | The material used for rendering the gaussians. It has a set of [parameters](#mat_parameters) that can be changed to adjust the look of the 3DGS render. |
| Gaussian Utils Material | Utility material used to initialize Gaussian data and calculate distance from camera for each Gaussian splat. |
| Sorting Material | Material used for Gaussian sorting based on their distance from the camera. |
| SH Order | The spherical harmonics order used for rendering the gaussians, which determines the visual fidelity of the gaussians. The SH order equal to zero represents only the perspective-indepent base color, while higher orders encode more persective-dependent details, such as shininess, reflections, etc. |
| SH Compression | Compressing the spherical harmonics data to improve VRAM usage efficiency. |
| Sort Interval | Frame interval between sorting gaussian splats. Higher values can be set for a relatively static camera to save performance. |
| Sort VR Per Eye | Enabling sorting for each eye in VR. If disabled, the Gaussian data will be sorted once and reused for the other eye. |


### Gaussian Material Parameters


The `gaussian_render` material contains the following parameters available for adjustment:


![](gaussian_material_settings.png)


| Sh Only | Using only the spherical harmonics components with the order > 0, i.e. without the base color, to see how they affect the final color. |
|---|---|
| Debug Cubes | Rendering primitive quads instead of guassians. Intended for debugging purposes. |
| Mip Splatting Paper |  |
| Use 2D Mip Filter | Enables the experimental 2D mip filter. The filter compensates for the alpha falloff of Gaussians at distance and applies screen-space blur depending on their projected size. This helps reduce aliasing and shimmering compared to the standard constant Gaussian size. |
| Mip Filter Scale | Controls the strength of the 2D mip filter. Lower values reduce the amount of blur applied to distant Gaussians, while higher values increase smoothing. This parameter can be used to fine-tune the visual sharpness when *Use 2D Mip Filter* is enabled. |
| Use Adaptive 3D Filter | Enables an experimental 3D smoothing filter for Gaussians. The filter adapts Gaussian scaling based on camera parameters used during training. It helps preserve small details such as grass, wires, and thin structures when viewed up close. |
| Training Minimum Depth | Specifies the minimum distance from the camera to Gaussians used during training. This value is required for the adaptive 3D filter to correctly scale Gaussians at close range. The value can be estimated from the dataset or adjusted manually for better visual results. |
| Training Maximum Focal Length | Specifies the maximum camera focal length used during training. This parameter is used by the adaptive 3D filter to reconstruct the appropriate Gaussian scale relative to the original capture cameras. The value can be approximated if the exact training parameters are unknown. |
| Splat Adjustments |  |
| Culling Frustum Dilation | The scalar factor controlling how much the view frustum is expanded to include 3D Gaussians whose centers lie outside the original frustum but remain visible due to their large covariance. |
| Culling Alpha Threshold | The alpha threshold for culling gaussians. Gaussians with alpha lower than this value will not be drawn. |
| Splat Opacity Scale | The gaussian splat opacity (alpha) scale factor. Adjust this value to make the gaussians more/less opaque. |
| Splat Size Scale | The gaussian splat size scale. Adjust this value to make the gaussians larger/smaller. |
| Low Pass Filter Strength | Controls the strength of the low-pass filter applied to Gaussians. Increasing this value increases blur and reduces high-frequency detail. This parameter is used when *[Use 2D Mip Filter](#use_2d_map_filter)* is disabled and provides a simpler way to smooth distant splats and reduce aliasing. |
| Color Adjustments |  |
| Tint Color | Applies a color tint to all splats. This parameter can be used to globally shift the color balance of the rendered Gaussians. |
| Temperature | Adjusts the color temperature of splats, shifting colors toward warmer (yellow/orange) or cooler (blue) tones. |
| Saturation | Controls the color saturation of splats. Higher values increase color intensity, while lower values move colors toward grayscale. |
| Brightness | Adjusts the overall brightness of splats. |
| Black Point | Defines the black level of the splat colors. Increasing this value darkens shadows and increases contrast. |
| White Point | Defines the white level of the splat colors. Adjusting this value changes the intensity of highlights. |
