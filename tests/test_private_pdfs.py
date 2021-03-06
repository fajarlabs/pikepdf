import pytest
from pikepdf import Pdf, PdfError

import os
import platform
import shutil
from contextlib import suppress
from shutil import copy
import gzip
from pathlib import Path


# Files with unknown copyright status can't be shared publicly
PRIVATE_RESOURCES = Path(__file__).parent / 'resources' / 'private'


@pytest.fixture
def private():
    return Path(PRIVATE_RESOURCES)


pytestmark = pytest.mark.skipif(
    not PRIVATE_RESOURCES.is_dir(),
    reason='private resources not available')


def test_pypdf2_issue_361(private):
    with gzip.open(str(private / 'pypdf2_issue_361.pdf.gz'), 'rb') as gz:
        with pytest.raises(PdfError, match=r'trailer'):
            Pdf.open(gz)
