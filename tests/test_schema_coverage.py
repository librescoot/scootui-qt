import json
from pathlib import Path
import subprocess
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/check-schema-coverage.sh"


class SchemaCoverageTest(unittest.TestCase):
    def run_check(self, schema, source="// fixture\n"):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            schema_path = root / "schema.json"
            schema_path.write_text(json.dumps(schema))
            source_dir = root / "src"
            source_dir.mkdir()
            (source_dir / "fixture.cpp").write_text(source)
            return subprocess.run(
                [str(SCRIPT), "--schema", str(schema_path), "--src", str(source_dir)],
                capture_output=True, text=True, check=False,
            )

    def test_local_only_browser_does_not_require_dashboard_annotation(self):
        result = self.run_check({"bluetooth.data-browser": {"user-visible": True, "read-only": True}})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_writable_browser_requires_annotation(self):
        result = self.run_check({"bluetooth.data-browser": {"user-visible": True, "read-only": False}})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("bluetooth.data-browser", result.stdout)

    def test_other_read_only_visible_setting_requires_annotation(self):
        result = self.run_check({"example.read-only": {"user-visible": True, "read-only": True}})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("example.read-only", result.stdout)

    def test_visible_annotated_setting_passes(self):
        result = self.run_check({"dashboard.theme": {"user-visible": True}}, "// @schema dashboard.theme\n")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
