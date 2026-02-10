# Pebble Waf build script
# WORKING VERSION for Rebble Cloud SDK (Feb 2026)

top = '.'
out = 'build'

def options(ctx):
    ctx.load('pebble_sdk')

def configure(ctx):
    ctx.load('pebble_sdk')

def build(ctx):
    ctx.load('pebble_sdk')

    binaries = []

    for p in ctx.env.TARGET_PLATFORMS:
        ctx.set_env(ctx.all_envs[p])
        ctx.set_group(ctx.env.PLATFORM_NAME)

        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        ctx.pbl_program(source=ctx.path.ant_glob('src/**/*.c'),
                        includes=['src'],
                        target=app_elf)

        # IMPORTANT: binaries must be list of dicts with 'platform' and 'app_elf' keys
        binaries.append({
            'platform': p,
            'app_elf': app_elf
        })

    # Bundle all platforms together
    ctx.set_group('bundle')
    ctx(features='bundle',
        binaries=binaries,
        js=ctx.path.ant_glob('src/js/**/*.js'))
