#-------------------------------------------------------------------#
# Copyright (c) 2026 Sonu Sonkar.                                   #
# This source code is licensed under the MIT license found in the   #
# LICENSE file in the root directory of this source tree.           #
#-------------------------------------------------------------------#
import logging
import sys
import traceback
import os
import functools
from pathlib import Path
from contextlib import contextmanager

# Setup Centralized Logging
LOG_DIR = Path("logs")
LOG_DIR.mkdir(exist_ok=True)

logging.basicConfig(
    level=logging.ERROR,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(LOG_DIR / f"{Path(__file__).stem}.log", encoding='utf-8'),
        logging.StreamHandler(sys.stdout)
    ]
)

# Function Decorator
def handle_func_errors(func):
    """Decorator to wrap functions with standard error handling and logging."""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        logger = logging.getLogger(func.__module__)
        try:
            return func(*args, **kwargs)
        except Exception as e:
            # Get location (file and line)
            _, _, exc_tb = sys.exc_info()
            last_frame = traceback.extract_tb(exc_tb)[-1]
            file_name = os.path.basename(last_frame.filename)
            line_num = last_frame.lineno

            # Get error code and details
            code = getattr(e, 'code', lambda: 'UNKNOWN')()
            details = getattr(e, 'details', lambda: str(e))()
            err_msg = f"{file_name} | {line_num} | " + \
                        f"{code} | {details}"
            logger.error(err_msg)  # No stack trace
            sys.exit(1)
    return wrapper

# Class Decorator
def handle_class_errors(cls):
    """Wraps all methods (including __init__) in a class with the error handler."""
    for attr_name, attr_value in list(cls.__dict__.items()): # Added list() for safety
        if (callable(attr_value) and not attr_name.startswith('__')) \
            or attr_name == '__init__':
            setattr(cls, attr_name, handle_func_errors(attr_value))
    return cls

# Context Manager (For scripts with direct logic)
@contextmanager
def error_context(name="Direct run"):
    try:
        yield
    except Exception as e:
        # Get location (file and line)
        _, _, exc_tb = sys.exc_info()
        last_frame = traceback.extract_tb(exc_tb)[-1]
        file_name = os.path.basename(last_frame.filename)
        line_num = last_frame.lineno

        # Get error code and details
        code = getattr(e, 'code', lambda: 'UNKNOWN')()
        details = getattr(e, 'details', lambda: str(e))()
        logging.getLogger(name).error(f"{file_name} | {line_num} | {code} | {details}")
        sys.exit(1)
