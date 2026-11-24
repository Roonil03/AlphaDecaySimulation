import unittest
import json
import subprocess
from unittest.mock import patch
from fit import analyze, load_data, native_prediction
from pathlib import Path
from tempfile import TemporaryDirectory
from prepare import prepare

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
    def test_prepare_preserves_membership_and_source(self):
        with TemporaryDirectory() as temp:
            csv = Path(temp) / "data.csv"
            csv.write_text("id,split,log10_half_life_s,parent_z,parent_a,q_mev,l,radius_fm\na,calibration,1,84,212,9,0,9\n")
            data = prepare(csv, "v1", "source citation")
            self.assertEqual(data["cases"][0]["split"], "calibration")
            self.assertEqual(data["cases"][0]["request"]["parent_a"], 212)
            self.assertEqual(data["source"], "source citation")
    def test_example_loads(self):
        self.assertEqual(load_data(Path(__file__).parent / "data/example-v1.json")["dataset_version"], "example-1")
    def test_dataset_version_must_be_a_nonempty_string(self):
        with TemporaryDirectory() as temp:
            path = Path(temp) / "data.json"
            path.write_text('{"schema_version": 1, "dataset_version": 1, "cases": []}')
            with self.assertRaisesRegex(ValueError, "versioned dataset"):
                load_data(path)
    def test_dataset_shape_and_integer_schema_version(self):
        valid = {'schema_version': 1, 'dataset_version': 'v1', 'cases': [
            {'id': 'a', 'split': 'calibration', 'log10_half_life_s': 1, 'request': {}}]}
        invalid = [None, [], {**valid, 'schema_version': True}, {**valid, 'schema_version': 1.0},
                   {**valid, 'cases': None}, {**valid, 'cases': {}}, {**valid, 'cases': [None]}]
        for changes in ({'id': 1}, {'id': None}, {'id': ' '}, {'split': None},
                        {'log10_half_life_s': True}, {'log10_half_life_s': '1'},
                        {'log10_half_life_s': None}, {'log10_half_life_s': float('inf')},
                        {'request': []}, {'request': None}):
            invalid.append({**valid, 'cases': [{**valid['cases'][0], **changes}]})
        with TemporaryDirectory() as temp:
            path = Path(temp) / 'data.json'
            for data in invalid:
                with self.subTest(data=data):
                    path.write_text(json.dumps(data), encoding='utf-8')
                    with self.assertRaises(ValueError):
                        load_data(path)
    def test_fit_retains_provenance(self):
        data = {"dataset_version": "v1", "source": "citation", "description": "illustrative", "cases": [
            {"id": "a", "split": "calibration", "log10_half_life_s": 1, "request": {}}]}
        result = analyze(data, lambda _: 1)
        self.assertEqual((result["source"], result["description"]), ("citation", "illustrative"))
    def test_channel_overrides_cannot_enter_common_factor_fit(self):
        with self.assertRaisesRegex(ValueError, "single spherical channels"):
            native_prediction("unused", {"channels": [{"preformation": 0.5}]})

    def test_native_result_requires_version_one_object(self):
        for response in ({"log10_half_life_s": 1}, {"schema_version": 2, "log10_half_life_s": 1},
                         {"schema_version": True, "log10_half_life_s": 1},
                         {"schema_version": 1.0, "log10_half_life_s": 1}, [1], None):
            with self.subTest(response=response), patch('fit.subprocess.run', return_value=
                    subprocess.CompletedProcess([], 0, stdout=json.dumps(response))):
                with self.assertRaisesRegex(ValueError, 'unsupported native result version'):
                    native_prediction('native', {})

    def test_native_prediction_requires_finite_numeric_result(self):
        for value in (True, '1', None, float('inf')):
            with self.subTest(value=value), patch('fit.subprocess.run', return_value=
                    subprocess.CompletedProcess([], 0, stdout=json.dumps(
                        {'schema_version': 1, 'log10_half_life_s': value}))):
                with self.assertRaisesRegex(ValueError, 'native prediction must be finite'):
                    native_prediction('native', {})

    def test_native_normalization_is_sent_and_validated(self):
        response = {"schema_version": 1, "log10_half_life_s": 2.5}
        with patch('fit.subprocess.run', return_value=subprocess.CompletedProcess(
                [], 0, stdout=json.dumps(response))) as invoke:
            self.assertEqual(native_prediction('native', {'preformation': .02, 'assault_hz': 3}), 2.5)
        request = json.loads(invoke.call_args.kwargs['input'])
        self.assertEqual((request['preformation'], request['assault_hz']), (1.0, 1e21))

    def test_common_factor_constrained_optimum_and_validation_independence(self):
        data = {"dataset_version": "v1", "cases": [
            {"id": "a", "split": "calibration", "log10_half_life_s": 2, "request": {}},
            {"id": "b", "split": "calibration", "log10_half_life_s": 4, "request": {}},
            {"id": "v", "split": "validation", "log10_half_life_s": -100, "request": {}}]}
        self.assertAlmostEqual(analyze(data, lambda _: 1)['preformation'], .01)
        # When all measured lifetimes are shorter, the physical optimum is at P=1.
        self.assertEqual(analyze(data, lambda _: 5)['preformation'], 1)
if __name__ == "__main__": unittest.main()

