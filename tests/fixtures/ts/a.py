import os
from dataclasses import dataclass

@dataclass
class Point:
    x: int = 0  # the x coordinate

def total(items: list[int], *, scale=2.5) -> float:
    """Sum of items"""
    s = 0
    for i, v in enumerate(items):
        if v > 3 and not v % 2:
            s += v * scale
    return f"{s:.2f}" if s else None
