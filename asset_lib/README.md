# Asset Lib

## File Format

File has effectively three parts:
- a header with some data we need in order to read the rest of the file
- some binary data that contains descriptions of all the various components of our asset, 
- A raw binary blob that we index into using the information we've gathered from our components.

The header contains the following info as u32s:
- version -- Used for versioning... obviously
- num_desc_components -- Used to tell us how many asset component descriptions we've got to read.
- components_blob byte_size -- How big our components blob is.
- binary_blob byte_size -- Same but for how large out binary blob is

The components blob follows the header and is ordered in a specific way. The first `num_desc_components` is AssetComponentDesc. The rest of the blob is indexed by those component descs.

The components then describe binary data to be processed or uploaded to the GPU.
