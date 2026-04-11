from entities.products.portfolio import Portfolio


def monthly_fixed_burden_for_portfolio(portfolio: Portfolio | None) -> float:
    """
    Return the visible monthly fixed burden for one portfolio.

    Notes:
    - Mortgage monthly_payment already includes escrow.
    - Auto insurance should always count separately.
    - Home insurance only counts separately for non-mortgaged households.
    - Deferred student loans do not count as active monthly burden.
    - Quarterly tax is smoothed to a monthly equivalent.
    """
    if portfolio is None:
        return 0.0

    total = 0.0

    if portfolio.mortgage is not None:
        total += float(portfolio.mortgage.monthly_payment)

    if portfolio.auto_loan is not None:
        total += float(portfolio.auto_loan.monthly_payment)

    if portfolio.student_loan is not None and not portfolio.student_loan.in_deferment:
        total += float(portfolio.student_loan.monthly_payment)

    if portfolio.insurance is not None:
        ins = portfolio.insurance

        if ins.auto is not None:
            total += float(ins.auto.monthly_premium)

        if ins.home is not None and portfolio.mortgage is None:
            total += float(ins.home.monthly_premium)

        if ins.life is not None:
            total += float(ins.life.monthly_premium)

    if portfolio.tax is not None:
        total += float(portfolio.tax.quarterly_amount) / 3.0

    return total
