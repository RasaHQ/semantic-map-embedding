import json
import argparse
from typing import Text, Dict, List


def render_embedding(active: List[int], height: int, width: int) -> None:
    for row in range(height):
        print("|", end="")
        for col in range(width):
            i = row * width + col
            print("X" if i in active else " ", end="")
        print("|")


def run(semantic_map_file: Text) -> None:
    with open(semantic_map_file, "r") as file:
        data = json.load(file)

    note = data.get("Note", "")
    height = data.get("Height")
    width = data.get("Width")
    assume_lower_case = data.get("AssumeLowerCase")
    global_topology = data.get("GlobalTopology")
    local_topology = data.get("LocalTopology")
    readme = data.get("CreationReadme")
    creation_options = data.get("CreationOptions")
    embeddings: Dict[Text, List[int]] = data.get("Embeddings")

    assert embeddings
    assert height
    assert width

    print(f"Height: {height}")
    print(f"Width: {width}")

    query = input("Term to lookup: ")
    while query:
        active = embeddings.get(query.strip())
        if active:
            print(active)
            render_embedding(active, height, width)
        else:
            print(f"The term '{query}' is not in the vocabulary of this map.")
        print()
        query = input("Term to lookup: ")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="View a semantic map (given as json)"
    )
    parser.add_argument(
        "--smap",
        type=str,
        help="Path to json file that represents the map",
    )

    args = parser.parse_args()

    run(
        args.smap,
    )

