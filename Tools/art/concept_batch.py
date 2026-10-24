# Concept images for a list of creatures in one go (SDXL base 1.0 loaded once), plus a labelled 2x2 sheet each.
#   python concept_batch.py batch_quadrupeds.json [--count 4] [--only id1,id2] [--seed 31]
import argparse, json, torch
from diffusers import StableDiffusionXLPipeline
from PIL import Image, ImageDraw

p = argparse.ArgumentParser()
p.add_argument("batch"); p.add_argument("--count", type=int, default=4); p.add_argument("--only", default="")
p.add_argument("--seed", type=int, default=31)
a = p.parse_args()
items = json.load(open(a.batch))["creatures"]
if a.only:
    items = [c for c in items if c["id"] in a.only.split(",")]

pipe = StableDiffusionXLPipeline.from_pretrained("stabilityai/stable-diffusion-xl-base-1.0", torch_dtype=torch.float16,
                                                 variant="fp16", use_safetensors=True)
pipe.enable_model_cpu_offload()
negative = ("cropped, cut off, multiple creatures, text, watermark, frame, cartoon, blurry, deformed, extra limbs, "
            "busy background, scenery, ground clutter, photo of real animal")
for c in items:
    prompt = (f"fantasy creature concept art of {c['prompt']}, full body, side view facing right, standing on all four legs, "
              "entire creature visible, centered, realistic, detailed anatomy, isolated on plain white background, soft studio lighting")
    sheet = Image.new("RGB", (1024, 1024), "white")
    for i in range(a.count):
        img = pipe(prompt=prompt, negative_prompt=negative, num_inference_steps=30, guidance_scale=7.0,
                   width=1024, height=1024, generator=torch.manual_seed(a.seed + i)).images[0]
        img.save(f"out/{c['id']}_{i}.png")
        tile = img.resize((512, 512))
        ImageDraw.Draw(tile).text((10, 10), str(i), fill="red")
        sheet.paste(tile, ((i % 2) * 512, (i // 2) * 512))
    sheet.save(f"out/{c['id']}_sheet.png")
    print("concepts", c["id"], flush=True)
