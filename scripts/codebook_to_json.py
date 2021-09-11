import json
from pathlib import Path

import numpy as np
import argparse
from typing import Text, Optional, List, Dict, Any, Set, BinaryIO


class Codebook:

    def __init__(self, directoryname: Text):
        codebook_filename = Path(directoryname) / "codebook.bin"
        print(f"Reading codebook from {codebook_filename}")
        with open(codebook_filename, "br") as codebook:
            self.format = int(np.fromfile(codebook, dtype=np.uint8, count=1)[0])
            assert self.format == 0
            self.height = int(np.fromfile(codebook, dtype=np.uint64, count=1)[0])
            self.width = int(np.fromfile(codebook, dtype=np.uint64, count=1)[0])
            self.vocab_size = int(np.fromfile(codebook, dtype=np.uint64, count=1)[0])
            self._codebook = np.reshape(
                np.fromfile(codebook, dtype=np.float32, count=-1),
                (self.height, self.width, self.vocab_size)
            )
        readme_filename = Path(directoryname) / "README.md"
        print(f"Reading README from {readme_filename}")
        self.readme = ""
        with open(readme_filename, "r") as readme:
            for line in readme:
                self.readme += line
                if line.startswith("Local topology:"):
                    self.local_topology = line[len("Local topology:"):].strip()
                elif line.startswith("Global topology:"):
                    self.global_topology = line[len("Global topology:"):].strip()

    def __str__(self) -> str:
        return f"Codebook({np.shape(self._codebook)})"

    def fingerprint(self, i: int, size: int) -> List[int]:
        assert 0 <= i < self.vocab_size
        assert 0 < size < self.vocab_size
        weights = np.reshape(
            self._codebook[:, :, i],
            (-1, )
        )
        # Return the `size` largest values
        return np.argsort(weights)[:size].tolist()


def run(
    codebook_filename: Text,
    vocabulary_filename: Text,
    output_filename: Text
) -> None:
    codebook = Codebook(codebook_filename)
    vocab: List[Text] = []
    with open(vocabulary_filename, "r", encoding="utf-8") as file:
        for word in file:
            vocab.append(word.strip())
    print("Extracting semantic map...")
    semantic_map = dict()
    size = max(codebook.width * codebook.height // 50, 5)
    for i, word in enumerate(vocab):
        semantic_map[word] = codebook.fingerprint(i, size)
    print(f"Exporting to {output_filename}")
    with open(output_filename, "w+") as file:
        json.dump(
            {
                "Note": "",
                "Height": codebook.height,
                "Width": codebook.width,
                "AssumeLowerCase": all([word.islower() for word in vocab]),
                "GlobalTopology": codebook.global_topology,
                "LocalTopology": codebook.local_topology,
                "CreationReadme": codebook.readme,
                "CreationOptions": {"MaxActiveCells": size, "Executor": "PythonScript"},
                "Embeddings": semantic_map,
            },
            file
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Convert semantic map binary codebook into json"
    )
    parser.add_argument(
        "--codebook",
        type=str,
        help="Path to codebook binary",
    )
    parser.add_argument(
        "--vocabulary",
        type=str,
        help="Path to vocabulary list",
    )
    parser.add_argument(
        "--out",
        type=str,
        help="Name of the json file to create",
    )

    args = parser.parse_args()

    run(
        args.codebook,
        args.vocabulary,
        args.out,
    )

