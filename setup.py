#!/usr/bin/env python3
"""
Setup script for DISMv2
"""

from setuptools import setup, find_packages
from pathlib import Path

# Read the README file
readme_file = Path(__file__).parent / "README.md"
long_description = ""
if readme_file.exists():
    with open(readme_file, "r", encoding="utf-8") as f:
        long_description = f.read()

setup(
    name="dismv2",
    version="2.0.0",
    author="DISMv2 Project",
    author_email="dismv2@example.com",
    description="Deployment Image Servicing and Management Tool v2 - Cross-platform DISM implementation",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/fhjordskhkjel/DISMv2",
    py_modules=["dismv2"],
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: System Administrators",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Topic :: System :: Installation/Setup",
        "Topic :: System :: Systems Administration",
        "Topic :: Utilities",
    ],
    python_requires=">=3.6",
    install_requires=[
        # Core functionality uses only standard library
    ],
    extras_require={
        "full": [
            "libarchive-c>=2.9",
            "psutil>=5.8.0",
        ],
        "windows": [
            "pywin32>=227; sys_platform == 'win32'",
        ],
    },
    entry_points={
        "console_scripts": [
            "dismv2=dismv2:main",
            "dism=dismv2:main",  # Alias for compatibility
        ],
    },
    keywords="dism deployment image servicing management windows system administration",
    project_urls={
        "Bug Reports": "https://github.com/fhjordskhkjel/DISMv2/issues",
        "Source": "https://github.com/fhjordskhkjel/DISMv2",
        "Documentation": "https://github.com/fhjordskhkjel/DISMv2/wiki",
    },
)