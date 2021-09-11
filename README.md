# Semantic Map Embedding

This repository contains code to generate semantic map embeddings from plain text
documents. Each line in the text files corresponds to one snippet that is used as input
to the self-organizing map algorithm. Lines wrapped in `=` on either end are considered
headings. Lines wrapped in `==` are subheadings, and so on. You can find an example
under `./examples/example_1/corpus/blogpost_00.txt`.

## Setup

Create a new python environment. E.g. with Conda:
```bash
conda create --name <env> --file ./requirements.txt
```
Then compile the executable binary for the self-organizing map:
```bash
make release
chmod u+x build/smap
./build/smap
```
Note, that your CPU needs to support [OpenMP](https://en.wikipedia.org/wiki/OpenMP) for
this to work. If your CPU doesn't support OpenMP, you can try compiling the executable
with `make sequential` instead.

If compilation was successful,
```
./build/smap --version
```
should yield the version number of the executable.

## Building an Example Semantic Map

You find an example corpus and commands under `examples/example_1` of this repo.
```bash
cd examples/example_1
```
To allow the shell scripts to run, you may have to unlock them first, using `chmod`:
```bash
chmod u+x *.sh
```
The self-organizing map executable takes a binary file as input. You can generate this
binary file from the example corpus with the `text_to_binary.py` script, or by running
```bash
./binarize.sh
```
As a result, the `example_1.bin` file should be created. You can now use this file as
input to the `smap` executable and then use the `codebook_to_json.py` script to convert
the resulting codebook into a JSON semantic map. Both these steps are run by:
```bash
./train-codebook.sh
```
Specifically, this results in the `example_1a.json` file, which contains the activation
indices for all the words in the vocabulary and can be used by Rasa's
`SemanticMapFeaturizer` on the [rasa-nlu-examples repo](https://rasahq.github.io/rasa-nlu-examples/docs/featurizer/semantic_map/).

You can use the `view_smap` script to show primitive ASCII renderings of the map that
you created:
```bash
./view-smap.sh
```



## Testing

To test the C++ code, run
```bash
make tests
cd build
chmod u+x smap-tests
./smap-tests
```
