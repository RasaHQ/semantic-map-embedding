# Converts a text corpus that is formatted as
#
# = Title =
# Snippet...
# == Subtitle ==
# Snippet...
# Snippet...
# ...
#
# into a binary file readable by `smap`


import os
import re
import argparse
from typing import Text, Optional, List, Dict, Any, Set, BinaryIO
from collections import defaultdict
from flashtext import KeywordProcessor
import multiprocessing


OUTPUT_FORMAT_VERSION: int = 2
OUTPUT_FORMAT_VERSION_NO_WEIGHTS: int = 3
DEBUGING: bool = False

IGNORED_SECTIONS = {"resources", "references", "external links"}


alphabets = r"([A-Za-z])"
digits = r"([0-9])"
prefixes = r"(Mr|St|Mrs|Ms|Dr)[.]"
suffixes = r"(Inc|Ltd|Jr|Sr|Co)"
starters = r"(Mr|Mrs|Ms|Dr|He\s|She\s|It\s|They\s|Their\s|Our\s|We\s|But\s|However\s|That\s|This\s|Wherever)"
acronyms = r"([A-Z][.][A-Z][.](?:[A-Z][.])?)"
websites = r"[.](com|net|org|io|gov)"


def split_into_sentences(text: Text) -> List[Text]:
    """Based on https://stackoverflow.com/a/31505798"""
    text = " " + text + "  "
    text = text.replace("\n", " ")
    text = re.sub(digits + "[.]" + digits, "\\1<prd>\\2", text)
    text = re.sub(prefixes, "\\1<prd>", text)
    text = re.sub(websites, "<prd>\\1", text)
    if "Ph.D" in text:
        text = text.replace("Ph.D.", "Ph<prd>D<prd>")
    text = re.sub("\s" + alphabets + "[.] ", " \\1<prd> ", text)
    text = re.sub(acronyms + " " + starters, "\\1<stop> \\2", text)
    text = re.sub(
        alphabets + "[.]" + alphabets + "[.]" + alphabets + "[.]",
        "\\1<prd>\\2<prd>\\3<prd>",
        text,
    )
    text = re.sub(alphabets + "[.]" + alphabets + "[.]", "\\1<prd>\\2<prd>", text)
    text = re.sub(" " + suffixes + "[.] " + starters, " \\1<stop> \\2", text)
    text = re.sub(" " + suffixes + "[.]", " \\1<prd>", text)
    text = re.sub(" " + alphabets + "[.]", " \\1<prd>", text)
    if "”" in text:
        text = text.replace(".”", "”.")
    if '"' in text:
        text = text.replace('."', '".')
    if "!" in text:
        text = text.replace('!"', '"!')
    if "?" in text:
        text = text.replace('?"', '"?')
    text = text.replace(".", ".<stop>")
    text = text.replace("?", "?<stop>")
    text = text.replace("!", "!<stop>")
    text = text.replace("<prd>", ".")
    sentences = text.split("<stop>")
    sentences = sentences[:-1]
    sentences = [s.strip() for s in sentences]
    return sentences


class CorpusStream:
    def __init__(
        self, filename: Text, num_cols: int, write_weights: bool = True
    ) -> None:
        self._filename = filename
        self._num_entries = 0
        self._num_rows = 0
        self._num_text_rows = 0
        self._num_cols = num_cols
        self._int_size = 4  # [uint32_t]
        self._weight_size = 1  # [uint8_t]
        self._byte_order = "little"
        self._file: Optional[BinaryIO] = None
        self._output_format_version = (
            OUTPUT_FORMAT_VERSION if write_weights else OUTPUT_FORMAT_VERSION_NO_WEIGHTS
        )

    def __del__(self) -> None:
        if self._file:
            self.flush()
            self._file.close()

    @property
    def file(self) -> BinaryIO:
        if not self._file:
            self._create_file()
        return self._file

    def _create_file(self) -> None:
        """
        Create a new file and write its header
        """
        self._file = open(self._filename, "wb+")
        # Format version [uint8_t]
        self._file.write(
            int(self._output_format_version).to_bytes(
                1, byteorder=self._byte_order, signed=False
            )
        )
        # Number of entries (filled on close) [uint64_t]
        self._file.write(int(0).to_bytes(8, byteorder=self._byte_order, signed=False))
        # Number of rows (filled on close) [uint32_t]
        self._file.write(int(0).to_bytes(4, byteorder=self._byte_order, signed=False))
        # Number of columns in the matrix [uint32_t]
        self._file.write(
            self._num_cols.to_bytes(4, byteorder=self._byte_order, signed=False)
        )

    def append_row(self, row: List[int], weights: Optional[List[int]] = None) -> None:
        assert weights is None or len(row) == len(weights)

        # Don't append empty rows
        if not row:
            return

        if DEBUGING:
            print(f"adding {row} with weights {weights}")

        num_entries_in_row = len(row)
        self._num_entries += num_entries_in_row
        self._num_rows += 1

        self.file.write(
            num_entries_in_row.to_bytes(
                self._int_size, byteorder=self._byte_order, signed=False
            )
        )
        for i in row:
            self.file.write(
                int(i).to_bytes(
                    self._int_size, byteorder=self._byte_order, signed=False
                )
            )
        if weights:
            for i in weights:
                self.file.write(
                    int(i).to_bytes(
                        self._weight_size, byteorder=self._byte_order, signed=False
                    )
                )

    def flush(self) -> None:
        if DEBUGING:
            print("flushing stream")
        # Go to beginning of file (but after 1-byte format version)
        self.file.flush()
        self.file.seek(1)

        # Number of entries [uint64_t]
        self.file.write(
            int(self._num_entries).to_bytes(8, byteorder=self._byte_order, signed=False)
        )
        # Number of rows [uint32_t]
        self.file.write(
            int(self._num_rows).to_bytes(4, byteorder=self._byte_order, signed=False)
        )

        # Go back to end of file
        self.file.flush()
        self.file.seek(0, 2)

    def append_corpus(self, filename: Text) -> None:
        with open(filename, "rb") as corpus:
            format_version = int.from_bytes(
                corpus.read(1), byteorder=self._byte_order, signed=False
            )
            if format_version != int(self._output_format_version):
                raise IOError(
                    f"File {filename} has incompatible format version {format_version}"
                )
            num_entries = int.from_bytes(
                corpus.read(8), byteorder=self._byte_order, signed=False
            )
            num_rows = int.from_bytes(
                corpus.read(4), byteorder=self._byte_order, signed=False
            )
            num_cols = int.from_bytes(
                corpus.read(4), byteorder=self._byte_order, signed=False
            )
            if self._num_cols != 0 and num_cols != self._num_cols:
                raise IOError(
                    f"File {filename} has incompatible column count {num_cols}"
                )
            self._num_cols = num_cols

            self.file.write(corpus.read())
            self._num_entries += num_entries
            self._num_rows += num_rows
            self.flush()

            if DEBUGING:
                print(
                    f"Appending {num_rows} rows with {num_entries} entires in {num_cols} columns"
                )
                print(f"Total rows: {self._num_rows}")


class Vocabulary(dict):

    """A self-filling dictionary"""

    def __init__(self, *args, **kwargs):
        super(Vocabulary, self).__init__(*args, **kwargs)
        self.max_index = -1
        self._locked = False
        self._used_vocab = defaultdict(int)
        self._processor = KeywordProcessor(case_sensitive=False)

    def __getitem__(self, item):
        value = self.get(item)
        if value is None:
            if self._locked:
                return None
            self.max_index += 1
            self[item] = self.max_index
            value = self.max_index
        if self._locked:
            self._used_vocab[item] += 1
        return value

    @property
    def word_use_counts(self) -> Dict[Text, int]:
        return dict(self._used_vocab)

    def lock(self):
        self._locked = True
        self._processor.add_keywords_from_list(list(self.keys()))

    def unlock(self):
        self._locked = False

    def find_all_words(self, text: Text, enforce_lowercase: bool = True) -> List[Text]:
        assert self._locked

        if not enforce_lowercase:
            raise NotImplementedError("case sensitive")

        return self._processor.extract_keywords(text.replace("'", " '"))


class CorpusReader:
    def __init__(
        self, vocabulary_file: Optional[Text] = None, enforce_lowercase: bool = True
    ):
        self._vocabulary_file = vocabulary_file
        self._enforce_lowercase = enforce_lowercase
        self._vocabulary = Vocabulary()
        self._index_pointers = [0]
        self._num_snippets_encoded = 0

        if vocabulary_file:
            with open(vocabulary_file, "r", encoding="utf-8") as file:
                for word in file:
                    if self._enforce_lowercase:
                        word = word.lower()
                    self._vocabulary[word.strip()]
            self._vocabulary.lock()
            print(f"Using fixed vocabulary of {len(self._vocabulary)} words.")

    def read_line(self, line: Text) -> None:
        pass

    def word_index(self, word: Text) -> Optional[int]:
        self._vocabulary.lock()
        return self._vocabulary[word]

    @property
    def vocabulary(self) -> Dict[Text, int]:
        return self._vocabulary

    def flush(self) -> None:
        pass

    @property
    def num_snippets_encoded(self) -> int:
        return self._num_snippets_encoded


class TitlePrependingHierarchicalCorpusBinaryReader(CorpusReader):
    def __init__(
        self,
        output_file_name: Text,
        vocabulary_file: Optional[Text] = None,
        enforce_lowercase: bool = True,
        use_weights: bool = True,
        combine_lists: bool = True,
    ):
        super(TitlePrependingHierarchicalCorpusBinaryReader, self).__init__(
            vocabulary_file, enforce_lowercase
        )
        self._document_title_codes = set()
        self._section_title_codes = set()
        self._subsection_title_codes = set()
        self._subsubsection_title_codes = set()
        self._subsubsubsection_title_codes = set()
        self._recent_paragraph_codes = set()
        self._list_item_codes = set()
        self._use_weights = use_weights
        self._combine_lists = combine_lists
        self._current_section_title: str = ""

        self._skip_threshold: Optional[int] = None

        self._num_droped = 0
        self.snippet_length_counts = defaultdict(int)

        self._stream = CorpusStream(
            output_file_name, num_cols=len(self.vocabulary), write_weights=use_weights
        )

    def read_line(self, line: Text) -> None:
        line = line.strip()
        if DEBUGING:
            print(line)
        indices = set()
        all_words_of_line = self.vocabulary.find_all_words(
            line, self._enforce_lowercase
        )
        for word in all_words_of_line:
            word_index = self._vocabulary[word]
            if word_index is not None:
                indices.add(word_index)

        if self._list_item_codes and not is_list_item(line):
            row_indices = self._get_indices()
            row_weights = (
                [self._get_weight(i) for i in row_indices]
                if self._use_weights
                else None
            )
            self.snippet_length_counts[len(row_indices)] += 1
            self._stream.append_row(row_indices, row_weights)
            self._num_snippets_encoded += 1

        if is_start_of_document(line):
            self._document_title_codes = indices
            self._current_section_title = ""

            self._section_title_codes.clear()
            self._subsection_title_codes.clear()
            self._subsubsection_title_codes.clear()
            self._subsubsubsection_title_codes.clear()
            self._recent_paragraph_codes.clear()
            self._list_item_codes.clear()
        elif is_start_of_section(line):
            self._section_title_codes = indices
            self._current_section_title = line.lower()

            self._subsection_title_codes.clear()
            self._subsubsection_title_codes.clear()
            self._subsubsubsection_title_codes.clear()
            self._recent_paragraph_codes.clear()
            self._list_item_codes.clear()
        elif is_start_of_subsection(line, 3):
            self._subsection_title_codes = indices

            self._subsubsection_title_codes.clear()
            self._subsubsubsection_title_codes.clear()
            self._recent_paragraph_codes.clear()
            self._list_item_codes.clear()
        elif is_start_of_subsection(line, 4):
            self._subsubsection_title_codes = indices

            self._subsubsubsection_title_codes.clear()
            self._recent_paragraph_codes.clear()
            self._list_item_codes.clear()
        elif is_start_of_subsection(line, 5):
            self._subsubsubsection_title_codes = indices

            self._recent_paragraph_codes.clear()
            self._list_item_codes.clear()
        elif indices and self._combine_lists and is_list_item(line):
            self._list_item_codes.update(indices)
        elif indices and not self._current_section_should_be_ignored():
            self._recent_paragraph_codes = indices

            row_indices = self._get_indices()
            row_weights = (
                [self._get_weight(i) for i in row_indices]
                if self._use_weights
                else None
            )
            self.snippet_length_counts[len(row_indices)] += 1
            self._stream.append_row(row_indices, row_weights)
            self._num_snippets_encoded += 1
        else:
            self._skip_threshold = self.current_level_weight  # ToDo: Unused

    def _current_section_should_be_ignored(self) -> bool:
        return self._current_section_title in IGNORED_SECTIONS

    def _get_indices(self) -> List[int]:
        return sorted(
            list(
                {  # Sorting is strictly necessary for training vocabulary cutoff option in smap
                    *self._list_item_codes,
                    *self._recent_paragraph_codes,
                    *self._document_title_codes,
                    *self._section_title_codes,
                    *self._subsection_title_codes,
                    *self._subsubsection_title_codes,
                    *self._subsubsubsection_title_codes,
                }
            )
        )

    def _get_weight(self, i: int) -> int:
        if i in self._document_title_codes:
            return 6
        elif i in self._section_title_codes:
            return 5
        elif i in self._subsection_title_codes:
            return 4
        elif i in self._subsubsection_title_codes:
            return 3
        elif i in self._subsubsubsection_title_codes:
            return 2
        else:
            return 1

    @property
    def current_level_weight(self) -> int:
        if self._subsubsubsection_title_codes:
            return 2
        elif self._subsubsection_title_codes:
            return 3
        elif self._subsection_title_codes:
            return 4
        elif self._section_title_codes:
            return 5
        elif self._document_title_codes:
            return 6
        else:
            return 6

    @property
    def num_droped(self):
        return self._num_droped

    def clear(self):
        self._index_pointers = [0]
        self._document_title_codes.clear()
        self._section_title_codes.clear()
        self._subsection_title_codes.clear()
        self._subsubsection_title_codes.clear()
        self._subsubsubsection_title_codes.clear()
        self._stream.flush()

    def flush(self) -> None:
        self._stream.flush()


class DocumentWiseCorpusBinaryReader(CorpusReader):
    def __init__(
        self,
        output_file_name: Text,
        vocabulary_file: Optional[Text] = None,
        enforce_lowercase: bool = True,
        *args,
        **kwargs,
    ):
        super(DocumentWiseCorpusBinaryReader, self).__init__(
            vocabulary_file, enforce_lowercase
        )
        self.snippet_length_counts = defaultdict(int)
        self._current_section_title: str = ""
        self._stream = CorpusStream(
            output_file_name, num_cols=len(self.vocabulary), write_weights=False
        )
        self._title_buffer = set()
        self._body_buffer = set()

    def read_line(self, line: Text) -> None:
        line = line.strip()
        if DEBUGING:
            print(line)
        indices = set()
        all_words_of_line = self.vocabulary.find_all_words(
            line, self._enforce_lowercase
        )
        for word in all_words_of_line:
            word_index = self._vocabulary[word]
            if word_index is not None:
                indices.add(word_index)

        if is_start_of_section(line):
            self._current_section_title = line.lower()

        if is_start_of_document(line):
            self.flush()
            self._title_buffer.update(indices)
        elif indices and not self._current_section_should_be_ignored():
            self._body_buffer.update(indices)

    def _current_section_should_be_ignored(self) -> bool:
        return self._current_section_title in IGNORED_SECTIONS

    def flush(self) -> None:
        indices = self._get_indices()
        self.snippet_length_counts[len(indices)] += 1

        if indices:
            if DEBUGING:
                print(f"indicees: {indices}")
            self._stream.append_row(indices, None)
            self._num_snippets_encoded += 1

        self._title_buffer.clear()
        self._body_buffer.clear()
        self._stream.flush()
        self._current_section_title = ""

    def _get_indices(self) -> List[int]:
        return sorted(
            list(
                {  # Sorting is strictly necessary for training vocabulary cutoff option in smap
                    *self._title_buffer,
                    *self._body_buffer,
                }
            )
        )


def extract_words_from_title(title: Text) -> List[Text]:
    return words_of_line(title.strip("= "))


def is_start_of_paragraph(line: Text) -> bool:
    return len(line.strip()) == 0 or line == "\n"


def is_start_of_section(line: Text) -> bool:
    return line.startswith("== ")


def is_start_of_subsection(line: Text, depth: int = 3) -> bool:
    return line.startswith("=" * depth + " ")


def is_list_item(line: Text) -> bool:
    return line.startswith("* ")


def is_start_of_document(line: Text) -> bool:
    return line.startswith("= ")


def words_of_line(line: Text) -> List[Text]:
    # Tokenizer from Rasa CountVectorizerFeaturizer
    return re.sub(
        # there is a space or an end of a string after it
        r"[^\w#@&]+(?=\s|$)|"
        # there is a space or beginning of a string before it
        # not followed by a number
        r"(\s|^)[^\w#@&]+(?=[^0-9\s])|"
        # not in between numbers and not . or @ or & or - or #
        # e.g. 10'000.00 or blabla@gmail.com
        # and not url characters
        r"(?<=[^0-9\s])[^\w._~:/?#\[\]()@!$&*+,;=-]+(?=[^0-9\s])",
        " ",
        line.strip("= "),
    ).split()


def file_system_scan(f: Any, dir_name: Text, file_extension: Text = ""):
    for root, dirs, files in os.walk(dir_name):
        for file in files:
            if (not file_extension) or re.match(file_extension, file):
                f(os.path.join(root, file))


def list_files(dir_name: Text, file_extension: Text = "") -> List[Text]:
    result = []
    file_system_scan(result.append, dir_name, file_extension)
    return result


def partition(alist: List[Any], n: int) -> List[List[Any]]:
    """Partition a list into n sublists"""
    assert n > 0
    length = len(alist)
    return [alist[i * length // n : (i + 1) * length // n] for i in range(n)]


def process_files(index: int, filenames: List[Text]) -> None:
    if not filenames:
        return
    outname = output_filename + f".part-{index}"
    print(
        f"Writing to {outname} in process {os.getpid()} (split_sentences={split_sentences})"
    )
    if merge_documents:
        encoder = DocumentWiseCorpusBinaryReader(
            outname,
            vocabulary_file=vocabulary_filename,
            enforce_lowercase=enforce_lower_case,
        )
    else:
        encoder = TitlePrependingHierarchicalCorpusBinaryReader(
            outname,
            vocabulary_file=vocabulary_filename,
            enforce_lowercase=enforce_lower_case,
            use_weights=use_weights,
            combine_lists=combine_lists,
        )
    for i, filename in enumerate(filenames):
        with open(filename, "r") as file:
            _line_number = 0
            for line in file:
                if split_sentences:
                    for sentence in split_into_sentences(line):
                        encoder.read_line(sentence)
                else:
                    encoder.read_line(line)
                _line_number += 1
                if _line_number % 5000000 == 0:
                    print(
                        f"Processed {_line_number} lines for {outname} in process {os.getpid()}"
                    )
            encoder.flush()
        if i % 10 == 1:
            print(
                f"Processed {i} of {len(filenames)} ({100. * i / len(filenames):.2f}%) files for {outname} in process {os.getpid()}"
            )
        if DEBUGING:
            break
    print(
        f"Done writing {encoder.num_snippets_encoded} snippets to {outname} in process {os.getpid()}"
    )


def run(
    output_filename: Text,
    data_directory_name: Text,
    data_filename_pattern: Text = r".*y\.txt",
    vocabulary_filename: Optional[Text] = None,
    enforce_lower_case: bool = True,
    max_processes: Optional[int] = None,
    use_weights: bool = True,
    combine_lists: bool = True,
    split_sentences: bool = False,
    merge_documents: bool = False,
):
    print(f"split_sentences = {split_sentences}")
    print(f"merge_documents = {merge_documents}")
    num_processes = (
        min(max_processes, multiprocessing.cpu_count())
        if max_processes
        else multiprocessing.cpu_count()
    )

    all_files = list_files(data_directory_name, data_filename_pattern)
    files_per_process = partition(all_files, num_processes)

    def make_names_available(
        _output_filename,
        _vocabulary_filename,
        _enforce_lower_case,
        _use_weights,
        _combine_lists,
        _split_sentences,
        _merge_documents,
    ) -> None:
        global output_filename, vocabulary_filename, enforce_lower_case, use_weights, combine_lists, split_sentences, merge_documents
        output_filename = _output_filename
        vocabulary_filename = _vocabulary_filename
        enforce_lower_case = _enforce_lower_case
        use_weights = _use_weights
        combine_lists = _combine_lists
        split_sentences = _split_sentences
        merge_documents = _merge_documents

    pool = multiprocessing.Pool(
        initializer=make_names_available,
        initargs=(
            output_filename,
            vocabulary_filename,
            enforce_lower_case,
            use_weights,
            combine_lists,
            split_sentences,
            merge_documents,
        ),
        processes=num_processes,
    )
    pool.starmap(process_files, enumerate(files_per_process))
    print("Parsing complete. Now combining files")
    corpus_stream = CorpusStream(
        output_filename, 0, write_weights=(use_weights and not merge_documents)
    )
    for part_filename in [
        f"{output_filename}.part-{index}" for index in range(len(files_per_process))
    ]:
        if os.path.exists(part_filename):
            corpus_stream.append_corpus(part_filename)
    print("Done.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Convert text into binary file for 'smap'"
    )
    parser.add_argument(
        "--corpus",
        type=str,
        help="Path to corpus directory",
    )
    parser.add_argument(
        "--pattern",
        default=r".*",
        help="Regex pattern of the corpus filenames",
    )
    parser.add_argument(
        "--out",
        type=str,
        help="Output filename",
    )
    parser.add_argument(
        "--vocabulary",
        type=str,
        default=None,
        help="Vocabulary filename (optional)",
    )
    parser.add_argument(
        "--case-sensitive",
        action="store_true",
        help="Read corpus case sensitive",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="Print details for every line read",
    )
    parser.add_argument(
        "--max-processes",
        type=int,
        default=None,
        help="Maximum number of processes to use (optional)",
    )
    parser.add_argument(
        "--no-weights",
        action="store_true",
        help="Do not export weights from titles",
    )
    parser.add_argument(
        "--split-sentences",
        action="store_true",
        help="Use sentence-level snippets",
    )
    parser.add_argument(
        "--merge-documents", action="store_true", help="Use document level snippets"
    )

    args = parser.parse_args()

    if args.debug:
        DEBUGING = True

    if args.no_weights:
        OUTPUT_FORMAT_VERSION = OUTPUT_FORMAT_VERSION_NO_WEIGHTS

    run(
        output_filename=args.out,
        data_directory_name=args.corpus,
        vocabulary_filename=args.vocabulary,
        enforce_lower_case=(not args.case_sensitive),
        data_filename_pattern=args.pattern,
        max_processes=args.max_processes,
        use_weights=(not args.no_weights),
        split_sentences=args.split_sentences,
        merge_documents=args.merge_documents,
    )
