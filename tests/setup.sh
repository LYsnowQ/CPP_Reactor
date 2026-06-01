#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VENV_DIR="${SCRIPT_DIR}/.venv"
REQUIREMENTS="${SCRIPT_DIR}/requirements.txt"

# 检测可用的 Python 3，优先用 python3.12
PYTHON=""
for cmd in python3.12 python3; do
    if command -v "$cmd" &>/dev/null; then
        PYTHON="$cmd"
        break
    fi
done

if [ -z "$PYTHON" ]; then
    echo "[setup] 未找到 Python 3，请先安装: sudo apt install python3"
    exit 1
fi

echo "[setup] 使用 Python: $($PYTHON --version) ($(command -v $PYTHON))"

# 检查 venv 模块
if ! $PYTHON -c "import venv" &>/dev/null; then
    echo "[setup] Python venv 模块不可用，请安装:"
    echo "  sudo apt install python3-venv"
    echo ""
    echo "  或（如果使用 Python 3.12）:"
    echo "  sudo apt install python3.12-venv"
    exit 1
fi

echo "[setup] 创建虚拟环境 → ${VENV_DIR}"
rm -rf "${VENV_DIR}"
$PYTHON -m venv "${VENV_DIR}"

echo "[setup] 安装依赖"
source "${VENV_DIR}/bin/activate"
pip install --upgrade pip --quiet
pip install -r "${REQUIREMENTS}"

echo "[setup] 完成！"
echo ""
echo "  运行 seed_data.py:"
echo "  source ${VENV_DIR}/bin/activate && python3 ${SCRIPT_DIR}/seed_data.py"
echo ""
echo "  或一步执行:"
echo "  ${VENV_DIR}/bin/python3 ${SCRIPT_DIR}/seed_data.py"
