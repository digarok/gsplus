# need pil/pillow
# sudo pip install pillow
#  ^^ on mac osx
from PIL import Image
#im = Image.open("gsp_icon_128.png") #Can be many different formats.
im = Image.open("gsp_icon_256.png") #Can be many different formats.
pix = im.load()
print im.size #Get the width and hight of the image for iterating over
width = im.size[0]
height = im.size[1]
count = 0
for y in range(0,width):
  for x in range(0,height):
    # print pix[x,y]
    print("0x%02x%02x%02x%02x, " % pix[x,y]),
    # RRGGBBAA
    count = count + 1
    if count == 32:
      count = 0
      print 


#print pix[x,y] #Get the RGBA Value of the a pixel of an image
#pix[x,y] = value # Set the RGBA Value of the image (tuple)


