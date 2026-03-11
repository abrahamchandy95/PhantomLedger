from common.transactions import Transaction

from .policy import Typology
from .ring_plan import RingPlan
from .run_context import IllicitContext
from .typology_classic import inject_classic_typology
from .typology_funnel import inject_funnel_typology
from .typology_invoice import inject_invoice_typology
from .typology_layering import inject_layering_typology
from .typology_structuring import inject_structuring_typology


def inject_illicit_for_ring(
    ctx: IllicitContext,
    ring_plan: RingPlan,
    typology: Typology,
    budget: int,
) -> list[Transaction]:
    match typology:
        case "classic":
            return inject_classic_typology(ctx, ring_plan, budget)
        case "layering":
            return inject_layering_typology(ctx, ring_plan, budget)
        case "funnel":
            return inject_funnel_typology(ctx, ring_plan, budget)
        case "structuring":
            return inject_structuring_typology(ctx, ring_plan, budget)
        case "invoice":
            return inject_invoice_typology(ctx, ring_plan, budget)
