"""Make the captioned Google Play screenshots in docs/store-assets/play/.

Usage: python3 tools/play-screenshots.py [docs/screenshots] [docs/store-assets/play]

Each one is a 1920x1080 (16:9, at least 1080px, so eligible for Play's
promotions) navy frame with a caption over one of docs/screenshots/'s images,
in upload order. local-5player.png is a frame of a five-bot local game with
candy bubbles, rendered headless at 960x720 with text scale 1.5.
"""
import sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter
src = sys.argv[1] if len(sys.argv) > 1 else "docs/screenshots"
out = sys.argv[2] if len(sys.argv) > 2 else "docs/store-assets/play"
shots = [
 ("local-5player.png","Last board standing wins"),
 ("game-room.jpg","Online battles, up to 20 players"),
 ("modern-gameplay.jpg","Pop groups, send attack bubbles"),
 ("set-teams.png","Team up or go solo"),
 ("how-to-play.jpg","Swap: save a bubble for later"),
 ("level-cleared.jpg","100 classic solo levels"),
 ("weekly-rankings.png","Climb the weekly rankings"),
 ("local-4player.jpg","Play with touch, pad or keyboard"),
]
NAMES=["last-board-standing","online-battles","pop-groups","team-up","swap","solo-levels","weekly-rankings","touch-pad-keyboard"]
W,H=1920,1080; BAND=180
font=ImageFont.truetype("share/gfx/Baloo2-ExtraBold.ttf",92)
top=(18,36,70); bot=(3,8,20); ice=(127,214,255)
for i,(f,cap) in enumerate(shots,1):
    bg=Image.new("RGB",(W,H))
    d=ImageDraw.Draw(bg)
    for y in range(H):
        t=y/(H-1); d.line([(0,y),(W,y)],fill=tuple(int(top[k]*(1-t)+bot[k]*t) for k in range(3)))
    im=Image.open(f"{src}/{f}").convert("RGB")
    sh=H-BAND-44; sw=int(sh*im.width/im.height)
    im=im.resize((sw,sh),Image.LANCZOS)
    x=(W-sw)//2; y=BAND
    glow=Image.new("L",(W,H),0); ImageDraw.Draw(glow).rounded_rectangle([x-6,y-6,x+sw+6,y+sh+6],22,fill=255)
    glow=glow.filter(ImageFilter.GaussianBlur(18))
    bg=Image.composite(Image.new("RGB",(W,H),ice),bg,glow.point(lambda v:v*0.45))
    mask=Image.new("L",(sw,sh),0); ImageDraw.Draw(mask).rounded_rectangle([0,0,sw-1,sh-1],16,fill=255)
    bg.paste(im,(x,y),mask)
    d=ImageDraw.Draw(bg)
    d.rounded_rectangle([x-3,y-3,x+sw+2,y+sh+2],18,outline=ice,width=3)
    bx=d.textbbox((0,0),cap,font=font); tw=bx[2]-bx[0]
    d.text(((W-tw)//2-bx[0],(BAND-(bx[3]-bx[1]))//2-bx[1]-4),cap,font=font,fill=(255,255,255),stroke_width=4,stroke_fill=(6,20,40))
    bg.save(f"{out}/{i}-{NAMES[i-1]}.png",optimize=True)
