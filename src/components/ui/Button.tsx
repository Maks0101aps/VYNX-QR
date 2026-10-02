import type { JSX } from 'react';

import type { ButtonHTMLAttributes, ReactNode, Ref } from 'react';

import styles from './ui.module.css';

export type ButtonVariant = 'default' | 'primary' | 'subtle' | 'danger';

type ButtonProps = ButtonHTMLAttributes<HTMLButtonElement> & {
  variant?: ButtonVariant;
  small?: boolean;
  iconOnly?: boolean;
  children?: ReactNode;
  ref?: Ref<HTMLButtonElement>;
};

const VARIANT_CLASS: Record<ButtonVariant, string | undefined> = {
  default: undefined,
  primary: styles.primary,
  subtle: styles.subtle,
  danger: styles.danger,
};

export function Button({
  variant = 'default',
  small = false,
  iconOnly = false,
  className,
  type = 'button',
  ...rest
}: ButtonProps): JSX.Element {
  const classes = [
    styles.button,
    VARIANT_CLASS[variant],
    small ? styles.small : undefined,
    iconOnly ? styles.iconOnly : undefined,
    className,
  ]
    .filter(Boolean)
    .join(' ');

  return <button type={type} className={classes} {...rest} />;
}
