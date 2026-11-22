#!/usr/bin/env python3
"""Pegasus Supervisor: execução real por padrão; --simulate habilita bancada virtual."""

import argparse
import logging
from app.core.robot_controller import RobotController
from app.ui.main_window import MainWindow


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--simulate", action="store_true", help="Conectar ao robô SIMULADO"
    )
    parser.add_argument("--debug", action="store_true")
    args = parser.parse_args()
    logging.basicConfig(
        level=logging.DEBUG if args.debug else logging.INFO,
        format="%(asctime)s %(levelname)s %(message)s",
    )
    MainWindow(RobotController(), simulate=args.simulate).mainloop()


if __name__ == "__main__":
    main()
