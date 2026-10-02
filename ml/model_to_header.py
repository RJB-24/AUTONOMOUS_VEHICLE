import argparse
from pathlib import Path
p=argparse.ArgumentParser(); p.add_argument('model'); p.add_argument('output'); a=p.parse_args()
data=Path(a.model).read_bytes()
lines=[', '.join(f'0x{x:02x}' for x in data[i:i+12]) for i in range(0,len(data),12)]
text='#pragma once\n#include <stdint.h>\nalignas(16) const unsigned char g_model[] = {\n  '+',\n  '.join(lines)+'\n};\nconst unsigned int g_model_len = sizeof(g_model);\n'
Path(a.output).parent.mkdir(parents=True,exist_ok=True); Path(a.output).write_text(text)
