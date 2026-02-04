# Product Guidelines: Simple Polygon Renderer (SPR)

## Documentation & Communication Style
- **Technical & Precise:** Focus on clear explanations of algorithms and performance characteristics.
- **Approachable & Educational:** Aim to make the inner workings of software rendering accessible to developers of all levels. Code comments should explain *why* a particular approach was taken, not just *what* the code does.

## Visual Identity & UX
- **Clean & Functional:** Prioritize a straightforward user interface that gets out of the way of the 3D content. Use simple overlays for necessary information.
- **Low-Poly Aesthetic:** Embrace the visual characteristics of software-based rendering, focusing on clarity and correctness over photorealism.

## Error Handling & Reliability
- **Minimalist Error Management:** The core library should primarily return descriptive error codes. It is the responsibility of the calling application to handle these errors appropriately. This keeps the library core lean and avoids side effects like unexpected `stderr` spam.

## Contribution & Code Standards
- **Pattern Consistency:** Prioritize maintaining consistency with established project patterns and naming conventions over strict adherence to rigid external style guides.
- **Clarity over Optimization:** Maintain high code readability as the primary goal. The logic should be easy to follow and modify.

## Performance Philosophy
- **Readability First:** Favor clear, understandable code even at the expense of potential micro-optimizations. Optimization should only be pursued when it provides a significant and measurable benefit without significantly obscuring the underlying logic.
