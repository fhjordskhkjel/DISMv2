#!/usr/bin/env python3
"""
Test suite for DISMv2
"""

import unittest
import sys
import os
import tempfile
import shutil
from pathlib import Path

# Add the parent directory to Python path to import dismv2
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from dismv2 import DISMv2

class TestDISMv2(unittest.TestCase):
    """Test cases for DISMv2 functionality"""
    
    def setUp(self):
        """Set up test environment"""
        self.dism = DISMv2()
        self.test_dir = Path(tempfile.mkdtemp())
        self.test_image = self.test_dir / "test.wim"
        self.test_mount = self.test_dir / "mount"
        
        # Create a dummy test image file
        with open(self.test_image, 'wb') as f:
            f.write(b'0' * 1024)  # 1KB dummy file
    
    def tearDown(self):
        """Clean up test environment"""
        shutil.rmtree(self.test_dir)
    
    def test_mount_image(self):
        """Test image mounting functionality"""
        result = self.dism.mount_image(str(self.test_image), str(self.test_mount))
        self.assertTrue(result)
        self.assertTrue(self.test_mount.exists())
        self.assertIn(str(self.test_mount), self.dism.mounted_images)
    
    def test_mount_nonexistent_image(self):
        """Test mounting non-existent image"""
        with self.assertRaises(FileNotFoundError):
            self.dism.mount_image("nonexistent.wim", str(self.test_mount))
    
    def test_unmount_image(self):
        """Test image unmounting functionality"""
        # First mount an image
        self.dism.mount_image(str(self.test_image), str(self.test_mount))
        
        # Then unmount it
        result = self.dism.unmount_image(str(self.test_mount))
        self.assertTrue(result)
        self.assertNotIn(str(self.test_mount), self.dism.mounted_images)
    
    def test_unmount_unmounted_image(self):
        """Test unmounting an image that's not mounted"""
        with self.assertRaises(ValueError):
            self.dism.unmount_image(str(self.test_mount))
    
    def test_get_image_info(self):
        """Test getting image information"""
        # This should not raise an exception
        self.dism.get_image_info(str(self.test_image))
    
    def test_get_image_info_nonexistent(self):
        """Test getting info for non-existent image"""
        with self.assertRaises(FileNotFoundError):
            self.dism.get_image_info("nonexistent.wim")
    
    def test_get_features(self):
        """Test getting Windows features"""
        # This should not raise an exception
        self.dism.get_features("/Online")
    
    def test_enable_feature(self):
        """Test enabling a Windows feature"""
        # This should not raise an exception
        self.dism.enable_feature("/Online", "TestFeature")
    
    def test_disable_feature(self):
        """Test disabling a Windows feature"""
        # This should not raise an exception
        self.dism.disable_feature("/Online", "TestFeature")
    
    def test_get_packages(self):
        """Test getting package information"""
        # This should not raise an exception
        self.dism.get_packages("/Online")
    
    def test_get_drivers(self):
        """Test getting driver information"""
        # This should not raise an exception
        self.dism.get_drivers("/Online")

class TestArgumentParsing(unittest.TestCase):
    """Test cases for argument parsing"""
    
    def test_help_argument(self):
        """Test help argument parsing"""
        original_argv = sys.argv
        try:
            sys.argv = ['dismv2.py', '/?']
            # Import and run main should show help and exit with 0
            from dismv2 import main
            with self.assertRaises(SystemExit) as cm:
                main()
            self.assertEqual(cm.exception.code, 0)
        finally:
            sys.argv = original_argv
    
    def test_no_arguments(self):
        """Test behavior with no arguments"""
        original_argv = sys.argv
        try:
            sys.argv = ['dismv2.py']
            from dismv2 import main
            with self.assertRaises(SystemExit) as cm:
                main()
            self.assertEqual(cm.exception.code, 0)
        finally:
            sys.argv = original_argv

if __name__ == '__main__':
    # Run the tests
    unittest.main(verbosity=2)