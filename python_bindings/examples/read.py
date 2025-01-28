import asyncio
import sys
from ctypes import pointer
from multiprocessing import shared_memory

import cbor2
from fastapi import FastAPI, Request, WebSocket
from fastapi.templating import Jinja2Templates

from pyncctools import Message, ReadToken, RingBuffer, read_next

shm = shared_memory.SharedMemory(name="Testing", create=False, track=False)
ringbuffer = RingBuffer.from_buffer(shm.buf)
read_token = ReadToken(0, False)


app = FastAPI()
templates = Jinja2Templates(directory="templates")


@app.get("/")
def read_root(request: Request):
    return templates.TemplateResponse("index.htm", {"request": request})


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    while True:
        await asyncio.sleep(0.001)
        msg: Message = read_next(pointer(read_token), pointer(ringbuffer))
        if msg.wait:
            continue
        payload = cbor2.loads(msg.data)
        await websocket.send_json(payload)
