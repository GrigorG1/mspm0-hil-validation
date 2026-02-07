"""
Pytest configuration and fixtures for HIL tests.
"""

import pytest
import os
import logging
from datetime import datetime
from hil_client import HILClient

# ============ Configuration ============

# Default COM port - override with: pytest --port COM7
DEFAULT_PORT = "COM7"


def pytest_addoption(parser):
    """
    Add custom command-line options to pytest.
    
    This lets you run: pytest --port COM5
    instead of hardcoding the port.
    """
    parser.addoption(
        "--port",
        action="store",
        default=DEFAULT_PORT,
        help=f"Serial port for HIL device (default: {DEFAULT_PORT})"
    )


# ============ Logging Setup ============

def setup_logging(log_dir: str = "logs") -> logging.Logger:
    """
    Configure logging to both console and timestamped file.
    """
    # Create logs directory if it doesn't exist
    os.makedirs(log_dir, exist_ok=True)
    
    # Create timestamped filename: logs/hil_test_20240115_143022.log
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_file = os.path.join(log_dir, f"hil_test_{timestamp}.log")
    
    # Configure logger
    logger = logging.getLogger("hil_test")
    logger.setLevel(logging.DEBUG)
    
    # Avoid duplicate handlers if tests are run multiple times
    if logger.handlers:
        return logger
    
    # File handler - detailed logs
    file_handler = logging.FileHandler(log_file)
    file_handler.setLevel(logging.DEBUG)
    file_format = logging.Formatter('%(asctime)s | %(levelname)-8s | %(message)s')
    file_handler.setFormatter(file_format)
    
    # Console handler - less verbose
    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.INFO)
    console_format = logging.Formatter('%(levelname)-8s | %(message)s')
    console_handler.setFormatter(console_format)
    
    logger.addHandler(file_handler)
    logger.addHandler(console_handler)
    
    logger.info(f"Logging to: {log_file}")
    return logger


# ============ Fixtures ============

@pytest.fixture(scope="session")
def logger():
    """
    Provide a configured logger to all tests.
    """
    return setup_logging()


@pytest.fixture(scope="session")
def port(request):
    """
    Get the serial port from command line or use default.
    """
    return request.config.getoption("--port")


@pytest.fixture(scope="function")
def client(port, logger):
    """
    Provide a connected HIL client to each test.
    """
    logger.info(f"Connecting to {port}...")
    
    hil = HILClient(port)
    hil.connect()
    
    logger.info("Connected successfully")
    
    yield hil  # Run the test here, with 'hil' as the client
    
    # Cleanup after test completes
    logger.info("Disconnecting...")
    hil.disconnect()