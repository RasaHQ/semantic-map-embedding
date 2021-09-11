
DATA=example_1.bin
NAME=example_1a

echo "Running self-organizing map training..."
../../build/smap create ${DATA} 16 16 --directory . --name ${NAME} --epochs 100 --local-topology 6 --global-topology 0 --non-adaptive > ${NAME}.log
echo "Converting codebook to json..."
python3 ../../scripts/codebook_to_json.py --codebook ${NAME} --vocabulary ./vocab.txt --out ${NAME}.json --density 0.2
