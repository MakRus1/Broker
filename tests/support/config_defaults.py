"""Load the built-in values for configs imported by a testsuite service."""

import yaml


def imported_config_defaults(service_source_dir):
    manifest = yaml.safe_load((service_source_dir / 'service.yaml').read_text())
    names = set(manifest.get('configs', []))
    names.update(
        f'{client["service"].upper()}_HTTP_CLIENT_CONFIG'
        for client in manifest.get('clients', {}).values()
        if client['transport'] == 'http'
    )
    schemas = service_source_dir.parents[1] / 'schemas/configs'
    paths = {path.stem: path for path in schemas.rglob('*.yaml')}
    defaults = {}
    for name in sorted(names):
        schema = yaml.safe_load(paths[name].read_text())
        value = schema['default']
        if schema['mode'] == 'conditional':
            value = {'clauses': [], 'default': value}
        defaults[name] = value
    static_config = yaml.safe_load((service_source_dir / 'configs/static_config.yaml').read_text())
    static_defaults = static_config['components_manager']['components']['dynamic-config']['defaults']
    defaults.update({name: value for name, value in static_defaults.items() if name in names})
    return defaults
