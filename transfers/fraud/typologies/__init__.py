from common.transactions import Transaction

from ..engine import IllicitContext, Typology
from ..rings import Plan

from . import classic, funnel, invoice, layering, mule, structuring


def generate(
    ctx: IllicitContext,
    ring_plan: Plan,
    typology: Typology,
    budget: int,
) -> list[Transaction]:
    """Routes the fraud generation request to the selected typology."""
    match typology:
        case "classic":
            return classic.generate(ctx, ring_plan, budget)
        case "layering":
            return layering.generate(ctx, ring_plan, budget)
        case "funnel":
            return funnel.generate(ctx, ring_plan, budget)
        case "structuring":
            return structuring.generate(ctx, ring_plan, budget)
        case "invoice":
            return invoice.generate(ctx, ring_plan, budget)
        case "mule":
            return mule.generate(ctx, ring_plan, budget)
