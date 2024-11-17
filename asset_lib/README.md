# Asset Lib

## File Format

File has effectively three parts:
- a header with data we need in order to read the rest of the file
- some binary data that contains descriptions of all the various components of our asset, 
- A raw binary blob that we index into using the information we've gathered from our components.

The header contains the following info as u32s:
- version -- Used for versioning... obviously
- num_desc_components -- Used to tell us how many asset component descriptions we've got to read.
- components_blob byte_size -- How big our components blob is.
- binary_blob byte_size -- Same but for how large out binary blob is

The components blob follows the header and is ordered in a specific way. The first `num_desc_components` is AssetComponentDesc. The rest of the blob is indexed by those component descs.

The components then describe binary data to be processed or uploaded to the GPU.

TODO:
- [ ] Texture Support
- [ ] FBX support (Use tinyfbx?)
- [ ] Skeletons & Animation support (Just use Ozz? probably)

## Model Asset

Contains entire subscene graph for the model:
- Transformation hierarchy
- Submeshes
- Meshes (groups of Submehes)
- Buffers (vertex and index buffers)
- Materials
- Textures

## Texture Asset

For now these are just .DDS files, we don't use a custom format.

Eventually we may want to do fancier shit but for now it's cool.

## Buffers

We only need two types of buffers at the moment:
- Vertex data
- Index data

Our vertex data is laid out in a SoA such that the different attributes are bundled into different buffers

- Positions
- UVs
- Normals
- Blend Indices
- Blend Weights
- Color

As a result each `RenderableDesc` will need to store `VertexAttribute` data for each attribute it uses. This stores a `BufferView` as well as the `MeshAttribute` type so that we can reason about which stream we need to access/bind.

Eventually we may want to support arbitrary material data or something in our buffers as well, but we can put that aside for now.
