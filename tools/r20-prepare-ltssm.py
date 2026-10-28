#!/usr/bin/env python3
"""R20 read-only LTSSM telemetry patch generator. Never edits existing source."""
from pathlib import Path
from difflib import unified_diff
import json,hashlib
root=Path("/srv/mirbi/memory/projects/default-workspace/projects/x1steamos-kernel-port")
src=root/"r17-pca-fuse-linux-nx/drivers/pci/controller/pci-tegra.c"
original=src.read_text()
marker="#define TEGRA_PCIE_LINKUP_TIMEOUT\t200"
anchor='\tdev_info(dev,\n\t\t "x1-r13-link initial'
retry='retry:\n\t\tdev_info(dev,'
if not (original.count(marker)==1 and original.count(anchor)==1 and original.count(retry)==1):
    raise SystemExit("PRECONDITION_FAILED: source markers drifted, no changes")
helper="""/* R20: read-only LTSSM hardware diagnostics from L4T Tegra210. */
#define X1_R20_RP_LTSSM_DBGREG 0x00000e44
#define X1_R20_RP_LTSSM_TRACE_CONTROL 0x00000e50
#define X1_R20_RP_LTSSM_TRACE_STATUS 0x00000e54

static void x1_r20_ltssm_snapshot(struct tegra_pcie_port *port,
                                  const char *phase)
{
    dev_info(port->pcie->dev,
             "x1-r20-ltssm %s port=%u dbg=%08x trace_ctl=%08x trace_status=%08x\\n",
             phase, port->index,
             readl(port->base + X1_R20_RP_LTSSM_DBGREG),
             readl(port->base + X1_R20_RP_LTSSM_TRACE_CONTROL),
             readl(port->base + X1_R20_RP_LTSSM_TRACE_STATUS));
}
"""
updated=original.replace(marker, helper+"\n"+marker,1)
updated=updated.replace(anchor,'\tx1_r20_ltssm_snapshot(port, "initial");\n'+anchor,1)
updated=updated.replace(retry,'retry:\n\t\tx1_r20_ltssm_snapshot(port, "retry");\n\t\tdev_info(dev,',1)
if updated.count("x1_r20_ltssm_snapshot(")!=3:
    raise SystemExit("REPLACEMENT_FAILED")
name="drivers/pci/controller/pci-tegra.c"
patch="".join(unified_diff(original.splitlines(keepends=True),updated.splitlines(keepends=True),fromfile="a/"+name,tofile="b/"+name))
out=root/"r20-ltssm-readonly.patch"
out.write_text(patch)
(root/"r20-pci-tegra-proposed.c").write_text(updated)
report={"status":"OFFLINE_PATCH_PREPARED_NOT_BUILT_NOT_INSTALLED","source_sha256":hashlib.sha256(original.encode()).hexdigest(),"patch_sha256":hashlib.sha256(patch.encode()).hexdigest(),"read_only_registers":["0xE44","0xE50","0xE54"],"source_unchanged":src.read_text()==original,"patch_file":str(out),"new_snapshot_calls":2}
(root/"r20-ltssm-manifest.json").write_text(json.dumps(report,indent=2)+"\n")
print(json.dumps(report))
