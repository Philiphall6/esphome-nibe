"""Run inside the ESPHome container: python -m unittest discover -s tests."""
import importlib.util
import pathlib
import unittest

try:
    import esphome
except ImportError:
    esphome = None

@unittest.skipIf(esphome is None, 'ESPHome is required for schema validation')
class EcsConfigTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = pathlib.Path(__file__).resolve().parents[1] / 'components/nibegw/__init__.py'
        spec = importlib.util.spec_from_file_location('ecs_config_under_test', path)
        cls.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.module)

    def config(self):
        return {'ecs': [{'address': 2, 'observe_only': True}, {'address': 3, 'observe_only': False}],
                'acknowledge': [32, 3], 'constants': [], 'udp': {'ports': []}}

    def test_valid_passive_s2_active_s3(self):
        config = self.config()
        self.assertIs(self.module._validate_ecs_addresses(config), config)

    def test_conflicts(self):
        for section in ['acknowledge', 'constants', 'ports', 'duplicate']:
            with self.subTest(section=section):
                config = self.config()
                if section == 'acknowledge': config[section].append(2)
                elif section == 'constants': config[section].append({'address': 2})
                elif section == 'ports': config['udp']['ports'].append({'address': 2})
                else: config['ecs'].append({'address': 2, 'observe_only': False})
                with self.assertRaises(self.module.cv.Invalid):
                    self.module._validate_ecs_addresses(config)
