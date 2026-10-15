"""Automation-friendly map validation, conversion, routing, and Studio launcher."""
import argparse
import json
import sys
from pathlib import Path

from .ingest import import_map, MAX_BYTES


def main(argv=None):
    parser = argparse.ArgumentParser(prog='python -m xroute')
    commands = parser.add_subparsers(dest='command', required=True)
    for name in ('validate', 'convert', 'route'):
        cmd = commands.add_parser(name)
        cmd.add_argument('input', help='UTF-8 map path, or - for stdin')
        cmd.add_argument('--format', default='auto', choices=['auto', 'json', 'xml', 'csv', 'tsv', 'ascii'])
        cmd.add_argument('--cell-width-cm', type=int, default=350)
        cmd.add_argument('--cell-height-cm', type=int, default=200)
        if name == 'convert':
            cmd.add_argument('--to', choices=['json', 'xml'], default='json')
            cmd.add_argument('--output', '-o', help='Output file (defaults to stdout)')
        if name == 'route':
            cmd.add_argument('--start', type=int, nargs=2, required=True, metavar=('X', 'Y'))
            cmd.add_argument('--goal', type=int, nargs=2, required=True, metavar=('X', 'Y'))
    studio = commands.add_parser('studio')
    studio.add_argument('--port', type=int, default=8000)
    args = parser.parse_args(argv)
    if args.command == 'studio':
        from .server import start_server
        start_server(args.port)
        return 0
    try:
        if args.input == '-':
            content = sys.stdin.read(MAX_BYTES + 1)
        else:
            with open(args.input, encoding='utf-8-sig') as source:
                content = source.read(MAX_BYTES + 1)
        result = import_map(content, args.format, args.cell_width_cm, args.cell_height_cm)
        if args.command == 'validate':
            output = {'status': 'ok', 'format': result.format, 'rows': result.data['rows'],
                      'cols': result.data['cols'], 'warnings': result.warnings}
        elif args.command == 'route':
            from . import Warehouse
            with Warehouse.from_import(result) as wh:
                route = wh.find_path(args.start, args.goal)
            output = {'status': 'ok', 'route': vars(route), 'warnings': result.warnings}
        else:
            output = result.to_xml() if args.to == 'xml' else result.data
        text = output if isinstance(output, str) else json.dumps(output, indent=2)
        if getattr(args, 'output', None):
            Path(args.output).write_text(text + '\n', encoding='utf-8')
        else:
            print(text)
        return 0
    except (ValueError, OSError, RuntimeError) as exc:
        print(json.dumps({'status': 'error', 'message': str(exc)}), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
