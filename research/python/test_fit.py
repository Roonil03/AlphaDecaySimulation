import unittest
from fit import analyze, load_data
from pathlib import Path

class FitTest(unittest.TestCase):
    def test_fit_excludes_validation_and_fixes_assault(self):
        data = {"dataset_version":"test", "cases":[
            {"id":"a", "split":"calibration", "log10_half_life_s":3, "request":{}},
            {"id":"b", "split":"validation", "log10_half_life_s":20, "request":{}}]}
        result = analyze(data, lambda _: 1)
        self.assertAlmostEqual(result["preformation"], .01)
        self.assertEqual(result["calibration_ids"], ["a"])
        self.assertEqual(result["assault_hz"], 1e21)
        self.assertEqual(result["cases"][1]["predicted_log10_half_life_s"], 3)
    def test_example_loads(self):
        self.assertEqual(load_data(Path(__file__).parent / "data/example-v1.json")["dataset_version"], "example-1")
if __name__ == "__main__": unittest.main()
