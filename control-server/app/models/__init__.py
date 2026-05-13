"""ORM models. Importing here ensures they register with Base.metadata."""

from app.models.admin_action import AdminAction
from app.models.blocklist import BlocklistEntry
from app.models.file import FileRecord
from app.models.file_distribution import DistributionStatus, FileDistribution
from app.models.folder_assignment import FolderAssignment
from app.models.machine import Machine
from app.models.shared_folder import FolderStatus, SharedFolder
from app.models.submission import Submission

__all__ = [
    "AdminAction",
    "BlocklistEntry",
    "DistributionStatus",
    "FileDistribution",
    "FileRecord",
    "FolderAssignment",
    "FolderStatus",
    "Machine",
    "SharedFolder",
    "Submission",
]
